/**
 *  Demux.cpp
 *  ONScripter-RU
 *
 *  Contains Media Engine A/V and subtitle demultiplexor.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Media/Controller.hpp"
#include "Engine/Components/Async.hpp"

bool MediaProcController::MediaDemux::prepare(int videoStream, int audioStream, int subtitleStream) {
	packetQueueSemSpaces[VideoEntry] = SDL_CreateSemaphore(VideoPacketBufferSize);
	packetQueueSemSpaces[AudioEntry] = SDL_CreateSemaphore(AudioPacketBufferSize);

	packetQueueSemData[VideoEntry] = SDL_CreateSemaphore(0);
	packetQueueSemData[AudioEntry] = SDL_CreateSemaphore(0);

	streamIds[VideoEntry] = videoStream;
	streamIds[AudioEntry] = audioStream;
	streamIds[SubsEntry]  = subtitleStream;

	return true;
}

bool MediaProcController::MediaDemux::resetPacketQueue() {
	for (auto entry : {VideoEntry, AudioEntry}) {
		while (!packetQueue[entry].empty()) {
			AVPacket *pkt = packetQueue[entry].back();
			if (pkt) {
				av_packet_unref(pkt);
				av_packet_free(&pkt);
			}
			packetQueue[entry].pop_back();
		}
	}
	return true;
}

AVPacket *MediaProcController::MediaDemux::obtainPacket(MediaEntries index, bool &cacheReadStarted) {
	AVPacket *packet = nullptr;
	Lock lock(&packetQueue[index]);
	if (!packetQueue[index].empty()) {
		packet = packetQueue[index].front();
		packetQueue[index].pop_front();
		SDL_SemPost(packetQueueSemSpaces[index]);
		cacheReadStarted = packet == nullptr;
	}
	return packet;
}

void MediaProcController::MediaDemux::demultiplexStreams(long double videoTimeBase) {
	int read_result{0};
	bool demultiplexingComplete{false};
	size_t counter{0};

	do {
		SDL_AtomicLock(&async.loadPacketArraysQueue.resultsLock);

		// Are we allowed to continue?
		if (demultiplexingComplete || shouldFinish.load(std::memory_order_acquire) || async.threadShutdownRequested) {
			SDL_AtomicUnlock(&async.loadPacketArraysQueue.resultsLock);
			if (media.initVideoTimecodesLock)
				SDL_SemPost(media.initVideoTimecodesLock);
			break;
		}

		AVPacket *packet = av_packet_alloc();
		read_result      = av_read_frame(media.formatContext, packet);

		SDL_AtomicUnlock(&async.loadPacketArraysQueue.resultsLock);

		MediaEntries entry = InvalidEntry;
		if (read_result >= 0 && !(packet->flags & AV_PKT_FLAG_CORRUPT)) {
			media.getVideoTimecodes(counter, packet, videoTimeBase);
			for (auto &e : {VideoEntry, AudioEntry, SubsEntry}) {
				if (packet->stream_index == streamIds[e]) {
					entry = e;
					break;
				}
			}
		}

		pushPacket(entry, packet, read_result, demultiplexingComplete, videoTimeBase);

	} while (true);
}

void MediaProcController::MediaDemux::pushPacket(MediaEntries id, AVPacket *packet, int read_result, bool &demultiplexingComplete, long double videoTimeBase) {
	while (id >= 0 && id != SubsEntry && !onsWaitSemaphoreTimeout(packetQueueSemSpaces[id], 10)) {
		if (shouldFinish.load(std::memory_order_acquire) || async.threadShutdownRequested) {
			return;
		}
	}

	SDL_AtomicLock(&async.loadPacketArraysQueue.resultsLock);
	int seek_res{0};

	// Are we allowed to continue?
	if (shouldFinish.load(std::memory_order_acquire) || async.threadShutdownRequested) {
		SDL_AtomicUnlock(&async.loadPacketArraysQueue.resultsLock);
		return;
	}

	// Note, we don't need to flush codec buffers in that case
	if (media.loopVideo && read_result < 0) {
		seek_res = av_seek_frame(media.formatContext, streamIds[VideoEntry], static_cast<int64_t>(0) * videoTimeBase, AVSEEK_FLAG_BACKWARD);
		//seek_res = avformat_seek_file(media.formatContext, streamIds[VideoEntry], INT64_MIN, 0, INT64_MAX, 0);
	}

	if (read_result < 0) {
		av_packet_free(&packet);

		if (!media.loopVideo || seek_res < 0) {
			Lock lock1(&packetQueue[VideoEntry]);
			Lock lock2(&packetQueue[AudioEntry]);
			packetQueue[VideoEntry].push_back(nullptr);
			if (media.hasStream(AudioEntry))
				packetQueue[AudioEntry].push_back(nullptr);
			SDL_SemPost(packetQueueSemData[VideoEntry]);
			if (media.hasStream(AudioEntry))
				SDL_SemPost(packetQueueSemData[AudioEntry]);
			demultiplexingComplete = true;
		}

		SDL_AtomicUnlock(&async.loadPacketArraysQueue.resultsLock);
		return;
	}

	//if (packet)
	//	sendToLog(LogLevel::Info, "PKT %d dts %lld pts %lld\n",id,packet->dts,packet->pts);

	if (id == InvalidEntry) {
		av_packet_free(&packet);
	} else if (id != SubsEntry) {
		Lock lock(&packetQueue[id]);
		packetQueue[id].push_back(packet);
	} else if (id >= 0 && packet && packet->buf && packet->buf->size > 0) {
		media.processSubsData(reinterpret_cast<char *>(packet->buf->data), packet->buf->size);
	}

	if (packet && id != SubsEntry) {
		SDL_SemPost(packetQueueSemData[id]);
	} else if (packet) {
		av_packet_unref(packet);
		av_packet_free(&packet);
	}

	SDL_AtomicUnlock(&async.loadPacketArraysQueue.resultsLock);
}
