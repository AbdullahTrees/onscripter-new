/**
 *  AudioDecoder.cpp
 *  ONScripter-RU
 *
 *  Contains Media Engine audio decoder.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Media/Controller.hpp"

bool MediaProcController::AudioDecoder::initSwrContext(const AudioSpec &audioSpec) {
	AVChannelLayout fallbackInputLayout{};
	const AVChannelLayout *inputChannelLayout = &codecContext->ch_layout;
	if (!inputChannelLayout->nb_channels) {
		av_channel_layout_default(&fallbackInputLayout, audioSpec.channels);
		inputChannelLayout = &fallbackInputLayout;
	}

	if (codecContext->sample_rate != audioSpec.frequency ||
	    inputChannelLayout->nb_channels != audioSpec.channels ||
	    codecContext->sample_fmt != audioSpec.format ||
	    av_channel_layout_compare(inputChannelLayout, &audioSpec.channelLayout) != 0) {
		int err = swr_alloc_set_opts2(&swrContext,
		                              &audioSpec.channelLayout, audioSpec.format, audioSpec.frequency,
		                              inputChannelLayout, codecContext->sample_fmt, codecContext->sample_rate,
		                              0, nullptr);
		if (err < 0 || !swrContext) {
			swr_free(&swrContext);
			return false;
		}

		err = swr_init(swrContext);
		if (err < 0) {
			swr_free(&swrContext);
			return false;
		}
	}
	return true;
}

void MediaProcController::AudioDecoder::processFrame(MediaFrame &vf) {
	uint8_t *output{nullptr};
	uint32_t outputSize{0};

	if (swrContext) {
		int64_t out_samples = static_cast<int64_t>(av_rescale_rnd(swr_get_delay(swrContext, codecContext->sample_rate) + frame->nb_samples,
		                                                          media.audioSpec.frequency, codecContext->sample_rate, AV_ROUND_UP));
		//Warning: further out_samples usage may loose precision
		av_samples_alloc(&output, nullptr, media.audioSpec.channels, static_cast<int32_t>(out_samples), media.audioSpec.format, 0);
		out_samples = swr_convert(swrContext, &output, static_cast<int32_t>(out_samples),
		                          const_cast<const uint8_t **>(frame->data), frame->nb_samples);

		outputSize = av_samples_get_buffer_size(nullptr, media.audioSpec.channels,
		                                        static_cast<int32_t>(out_samples), media.audioSpec.format, 1);
	} else {
		AVSampleFormat frameFormat = static_cast<AVSampleFormat>(frame->format);
		int channels = frame->ch_layout.nb_channels ? frame->ch_layout.nb_channels : codecContext->ch_layout.nb_channels;
		if (!channels)
			channels = media.audioSpec.channels;
		outputSize = av_samples_get_buffer_size(nullptr, channels,
		                                        frame->nb_samples, frameFormat, 1);
		output     = static_cast<uint8_t *>(av_malloc(outputSize));
		std::memcpy(output, frame->data[0], outputSize);
	}
	vf.data        = output;
	vf.dataSize    = outputSize;
	vf.dataDeleter = [](uint8_t *d) {
		av_freep(&d);
	};
	vf.frameNumber = ++debugFrameNumber;
}
