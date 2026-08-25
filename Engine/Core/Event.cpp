/**
 *  Event.cpp
 *  ONScripter-RU
 *
 *  Event handler core code.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Core/ONScripter.hpp"
#include "Engine/Components/Async.hpp"
#include "Engine/Components/Joystick.hpp"
#include "Engine/Components/Window.hpp"
#include "Engine/Layers/Media.hpp"

#ifdef LINUX
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <numeric>

const uint32_t MAX_TOUCH_TAP_TIMESPAN{80};
const uint32_t MAX_TOUCH_SWIPE_TIMESPAN{300};
const int EVENT_QUEUE_IDLE_WAIT_MS{8};
const float MIN_AUTO_FPS{30.0f};
const float MAX_AUTO_FPS{360.0f};
const uint64_t NANOS_PER_MILLISECOND{1000000ULL};
const uint64_t STALE_FRAME_BASELINE_NS{250ULL * NANOS_PER_MILLISECOND};
const uint64_t FPS_DISPLAY_UPDATE_INTERVAL_NS{250ULL * NANOS_PER_MILLISECOND};
const uint64_t MAX_FRAME_TAIL_COMPENSATION_NS{2ULL * NANOS_PER_MILLISECOND};
const uint64_t PRECISE_SLEEP_GUARD_NS{1ULL * NANOS_PER_MILLISECOND};
const uint64_t PRECISE_SLEEP_MIN_NS{2ULL * NANOS_PER_MILLISECOND};
#if !defined(ONS_USE_SDL3)
const uint64_t COARSE_SLEEP_FRAME_MIN_NS{10ULL * NANOS_PER_MILLISECOND};
const uint64_t COARSE_SLEEP_REMAINING_MIN_NS{8ULL * NANOS_PER_MILLISECOND};
#endif
#if !defined(ONS_USE_SDL3)
const float TOUCH_ACTION_THRESHOLD_X = 0.1;
const float TOUCH_ACTION_THRESHOLD_Y = 0.15;
#endif

struct FramePacingTelemetry {
	uint64_t frames{0};
	uint64_t totalFrameNanos{0};
	uint64_t maxFrameNanos{0};
	uint64_t targetFrameNanos{0};
	uint64_t waitTargetNanos{0};
	uint64_t overshootFrames{0};
	uint64_t overshootNanos{0};
	uint64_t downtimePolls{0};
	uint64_t downtimeWorkPolls{0};
	uint64_t preciseSleepCalls{0};
	uint64_t coarseSleepCalls{0};
	uint64_t spinPolls{0};
	uint64_t fpsDisplaySamples{0};
	uint64_t fpsDisplayTotalFrameNanos{0};
	uint64_t fpsDisplayMaxFrameNanos{0};
	double lastFpsDisplayFrameMs{0};
	double lastFpsDisplayValue{0};
	double minFpsDisplayValue{0};
	double maxFpsDisplayValue{0};
};

static FramePacingTelemetry framePacingTelemetry;

static bool framePacingTelemetryEnabled() {
	const char *value = onsSDLGetEnv("ONS_SDL3_GPU_TELEMETRY");
	return value && *value && std::strcmp(value, "0") != 0;
}

enum {
	ONS_MUSIC_EVENT,
	ONS_SEQMUSIC_EVENT,
};

static Direction getDirection(SDL_Scancode code) {
	switch (code) {
		case SDL_SCANCODE_RIGHT:
		case SDL_SCANCODE_KP_6:
			return Direction::RIGHT;
		case SDL_SCANCODE_UP:
		case SDL_SCANCODE_KP_8:
			return Direction::UP;
		case SDL_SCANCODE_DOWN:
		case SDL_SCANCODE_KP_2:
			return Direction::DOWN;
		default:
			return Direction::LEFT;
	}
}

extern bool ext_music_play_once_flag;
bool ext_music_play_once_flag = false;

/* **************************************** *
 * Callback functions
 * **************************************** */

extern "C" void musicFinishCallback();
void seqmusicCallback(int sig);
extern "C" void waveCallback(int channel);

extern "C" void musicFinishCallback() {
	SDL_Event event;
	event.type      = SDL_USEREVENT;
	event.user.code = ONS_MUSIC_EVENT;
	SDL_PushEvent(&event);
}

void seqmusicCallback(int /*sig*/) {
#ifdef LINUX
	int status;
	wait(&status);
#endif
	if (!ext_music_play_once_flag) {
		SDL_Event event;
		event.type      = SDL_USEREVENT;
		event.user.code = ONS_SEQMUSIC_EVENT;
		SDL_PushEvent(&event);
	}
}

extern "C" void waveCallback(int channel) {
	SDL_Event event;
	event.type      = ONS_CHUNK_EVENT;
	event.user.code = channel;
	SDL_PushEvent(&event);
}

void ONScripter::flushEventSub(SDL_Event &event) {
	//event related to streaming media
	if (event.user.code == ONS_MUSIC_EVENT && event.type == SDL_USEREVENT) {
		if (music_play_loop_flag ||
		    (cd_play_loop_flag && !cdaudio_flag)) {
			stopBGM(true);
			if (music_file_name)
				playSoundThreaded(music_file_name, SOUND_MUSIC, true);
			else
				playCDAudio();
		} else {
			stopBGM(false);
		}
	} else if (event.user.code == ONS_SEQMUSIC_EVENT && event.type == SDL_USEREVENT) {
		ext_music_play_once_flag = !seqmusic_play_loop_flag;
		Mix_FreeMusic(seqmusic_info);
		playSequencedMusic(seqmusic_play_loop_flag);
	} else if (event.type == ONS_CHUNK_EVENT) { // for processing btntime2 and automode correctly
		uint32_t ch = event.user.code;
		if (wave_sample[ch]) {
			if (ch >= ONS_MIX_CHANNELS || !channel_preloaded[ch]) {
				//don't free preloaded channels
				wave_sample[ch] = nullptr;
			}
			if (ch == MIX_LOOPBGM_CHANNEL0 &&
			    loop_bgm_name[1] &&
			    wave_sample[MIX_LOOPBGM_CHANNEL1])
				Mix_PlayChannel(MIX_LOOPBGM_CHANNEL1,
				                wave_sample[MIX_LOOPBGM_CHANNEL1]->chunk, -1);
			if (ch == 0 && bgmdownmode_flag)
				setCurMusicVolume(music_volume);
		}
	}
}

static std::atomic<bool> eventsArrived{false};
static SDL_SpinLock fetchedEventQueueLock{0};

void ONScripter::flushEvent() {
	SDL_Event event{};

	while (!localEventQueue.empty() || updateEventQueue()) {
		event = localEventQueue.back();
		localEventQueue.pop_back();
		flushEventSub(event);
	}
}

static uint64_t highResolutionTicksNanos() {
	static const uint64_t frequency = SDL_GetPerformanceFrequency();
	return static_cast<uint64_t>((static_cast<long double>(SDL_GetPerformanceCounter()) * 1000000000.0L) /
	                             static_cast<long double>(frequency));
}

void ONScripter::handleSDLEvents() {
	updateEventQueue();

	// Process some checks before returning from runEventLoop (at least automode/voicewait related)
	SDL_Event event{};
	event.type      = ONS_UPKEEP_EVENT;
	event.user.code = -1;
	localEventQueue.emplace_front(event);

	// Make sure we return from runEventLoop when we run out of events
	event           = SDL_Event{};
	event.type      = ONS_EVENT_BATCH_END;
	event.user.code = -1;
	localEventQueue.emplace_front(event);

	runEventLoop();

	while (takeEventsOut(ONS_EVENT_BATCH_END))
		;
	while (takeEventsOut(ONS_UPKEEP_EVENT))
		;
}

bool ONScripter::takeEventsOut(uint32_t type) {
	auto it = localEventQueue.begin();
	bool has{false};
	while (it != localEventQueue.end()) {
		if (it->type == type) {
			it  = localEventQueue.erase(it);
			has = true;
		} else
			++it;
	}

	return has;
}

bool ONScripter::updateEventQueue() {
	if (!eventsArrived.load(std::memory_order_acquire))
		return false;

	SDL_AtomicLock(&fetchedEventQueueLock);
	if (fetchedEventQueue.empty()) {
		eventsArrived.store(false, std::memory_order_release);
		SDL_AtomicUnlock(&fetchedEventQueueLock);
		return false;
	}

	while (!fetchedEventQueue.empty()) {
		localEventQueue.emplace_front(fetchedEventQueue.back());
		fetchedEventQueue.pop_back();
	}

	eventsArrived.store(false, std::memory_order_release);
	SDL_AtomicUnlock(&fetchedEventQueueLock);
	return true;
}

void ONScripter::fetchEventsToQueue() {
	uint32_t lastTimeStamp{0};

	auto pushEvent = [this](const SDL_Event &event) {
		SDL_AtomicLock(&fetchedEventQueueLock);
		fetchedEventQueue.emplace_front(event);
		eventsArrived.store(true, std::memory_order_release);
		SDL_AtomicUnlock(&fetchedEventQueueLock);
	};

	auto pushFingerEvents = [this, &lastTimeStamp, pushEvent](bool force = false) {
		for (size_t i = 0; i < 2; ++i) {
			if (fingerEventActive[i]) {
				auto &fingerEvent = fingerEvents[i];
				//sendToLog(LogLevel::Error, "Pushing finger event %s force %d at %d by %d, current %d\n",
				//			fingerEvent.type == SDL_FINGERUP ? "up" : "down", force,
				//			fingerEvent.common.timestamp, lastTimeStamp, SDL_GetTicks());
				if (force || onsEventTimestampMs(fingerEvent) + MAX_TOUCH_TAP_TIMESPAN <
				                 (lastTimeStamp == 0 ? (lastTimeStamp = static_cast<uint32_t>(SDL_GetTicks())) : lastTimeStamp)) {
					pushEvent(fingerEvent);
					fingerEventActive[i] = false;
				}
			}
		}
	};

	SDL_Event event{};
	SDL_Event tmp_event{};

	while (SDL_WaitEventTimeout(&event, EVENT_QUEUE_IDLE_WAIT_MS)) {
		// ignore continuous SDL_MOUSEMOTION
		while (event.type == SDL_MOUSEMOTION) {
			if (SDL_PeepEvents(&tmp_event, 1, SDL_PEEKEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) == 0)
				break;
			if (tmp_event.type != SDL_MOUSEMOTION)
				break;
			SDL_PeepEvents(&tmp_event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
			event = tmp_event;
		}

		// group finger events
		bool queueEmpty{false};
		while (event.type == SDL_FINGERDOWN || event.type == SDL_FINGERUP) {
			const size_t fingerIndex = event.type == SDL_FINGERUP ? 1 : 0;
			auto &finger             = fingerEvents[fingerIndex];
			bool &fingerActive       = fingerEventActive[fingerIndex];
			//sendToLog(LogLevel::Error, "Found finger %s event %d from %lld (%f, %f, %f, %f, %f) current %d has %d fingers\n",
			//			event.type == SDL_FINGERUP ? "up" : "down",
			//			event->common.timestamp, event->tfinger.touchId, event->tfinger.x, event->tfinger.y,
			//			event->tfinger.dx, event->tfinger.dy, event->tfinger.pressure,
			//			fingerActive ? finger.common.timestamp : -1, fingerActive ? (uint32_t)onsTouchFingerId(finger.tfinger) : 0);
			if (fingerActive && onsEventTimestampMs(finger) + MAX_TOUCH_TAP_TIMESPAN >= onsEventTimestampMs(event)) {
				onsTouchFingerId(finger.tfinger)++;
			} else {
				if (fingerActive)
					pushFingerEvents(true);
				finger = event;
				fingerActive = true;
				onsTouchFingerId(finger.tfinger) = 1;
			}

			if (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) <= 0) {
				queueEmpty = true;
				break;
			}
		}

		if (!queueEmpty) {
			//sendToLog(LogLevel::Error, "Updating from %d last timestamp with %d current %d\n",
			//			event->type, event->common.timestamp, SDL_GetTicks());
			lastTimeStamp = onsEventTimestampMs(event);
			pushEvent(event);
		}
	}

	pushFingerEvents();
}

float ONScripter::effectiveRefreshRate() const {
	if (force_fps_override && game_fps > 0)
		return static_cast<float>(game_fps);

	float detected_fps = window.currentDisplayRefreshRate();
	if (detected_fps >= MIN_AUTO_FPS && detected_fps <= MAX_AUTO_FPS)
		return detected_fps;

	if (game_fps > 0)
		return static_cast<float>(game_fps);
	return DEFAULT_FPS;
}

double ONScripter::currentScriptFrameDeltaScale() const {
	const int authoredFps = game_fps > 0 ? game_fps : DEFAULT_FPS;
	if (current_game_state_advance_nanos == 0 || authoredFps <= 0)
		return 1.0;

	return static_cast<double>(current_game_state_advance_nanos) *
	       static_cast<double>(authoredFps) / 1000000000.0;
}

void ONScripter::toggleFpsOverlay() {
	fps_overlay_visible          = !fps_overlay_visible;
	fps_overlay_dirty            = true;
	fps_overlay_refresh_required = true;

	if (fps_overlay_text.empty())
		fps_overlay_text = "FPS: --.-";

	sendToLog(LogLevel::Info, "turned %s FPS overlay\n", fps_overlay_visible ? "on" : "off");
}

void ONScripter::updateFpsCounter(double frameMilliseconds) {
	if (frameMilliseconds <= 0.0)
		return;

	displayed_fps = 1000.0 / frameMilliseconds;

	static uint64_t lastOverlayUpdateNanos = 0;
	uint64_t nowNanos = highResolutionTicksNanos();
	if (!fps_overlay_text.empty() &&
	    nowNanos - lastOverlayUpdateNanos < FPS_DISPLAY_UPDATE_INTERVAL_NS)
		return;
	lastOverlayUpdateNanos = nowNanos;

	char label[32];
	std::snprintf(label, sizeof(label), "FPS: %.1f", displayed_fps);
	if (fps_overlay_text != label) {
		fps_overlay_text  = label;
		fps_overlay_dirty = true;
	}
}

void ONScripter::printFramePacingTelemetry() const {
	if (!framePacingTelemetryEnabled() || framePacingTelemetry.frames == 0)
		return;

	const double averageFrameMs = static_cast<double>(framePacingTelemetry.totalFrameNanos) /
	                              static_cast<double>(framePacingTelemetry.frames) /
	                              static_cast<double>(NANOS_PER_MILLISECOND);
	const double maxFrameMs = static_cast<double>(framePacingTelemetry.maxFrameNanos) /
	                          static_cast<double>(NANOS_PER_MILLISECOND);
	const double targetFrameMs = static_cast<double>(framePacingTelemetry.targetFrameNanos) /
	                             static_cast<double>(NANOS_PER_MILLISECOND);
	const double waitTargetMs = static_cast<double>(framePacingTelemetry.waitTargetNanos) /
	                            static_cast<double>(NANOS_PER_MILLISECOND);
	const double fpsDisplayAverageFrameMs = framePacingTelemetry.fpsDisplaySamples
	                                            ? static_cast<double>(framePacingTelemetry.fpsDisplayTotalFrameNanos) /
	                                                  static_cast<double>(framePacingTelemetry.fpsDisplaySamples) /
	                                                  static_cast<double>(NANOS_PER_MILLISECOND)
	                                            : 0.0;
	const double fpsDisplayMaxFrameMs = static_cast<double>(framePacingTelemetry.fpsDisplayMaxFrameNanos) /
	                                    static_cast<double>(NANOS_PER_MILLISECOND);
	std::printf("Frame pacing telemetry: frames=%llu average_frame_ms=%.3f max_frame_ms=%.3f "
	            "target_frame_ms=%.3f wait_target_ms=%.3f overshoot_frames=%llu overshoot_ms=%.3f "
	            "downtime_polls=%llu downtime_work_polls=%llu precise_sleep_calls=%llu "
	            "coarse_sleep_calls=%llu spin_polls=%llu fps_display_samples=%llu "
	            "fps_display_average_frame_ms=%.3f fps_display_max_frame_ms=%.3f "
	            "last_fps_display_frame_ms=%.3f last_fps_display_value=%.3f "
	            "min_fps_display_value=%.3f max_fps_display_value=%.3f\n",
	            static_cast<unsigned long long>(framePacingTelemetry.frames),
	            averageFrameMs,
	            maxFrameMs,
	            targetFrameMs,
	            waitTargetMs,
	            static_cast<unsigned long long>(framePacingTelemetry.overshootFrames),
	            static_cast<double>(framePacingTelemetry.overshootNanos) /
	                static_cast<double>(NANOS_PER_MILLISECOND),
	            static_cast<unsigned long long>(framePacingTelemetry.downtimePolls),
	            static_cast<unsigned long long>(framePacingTelemetry.downtimeWorkPolls),
	            static_cast<unsigned long long>(framePacingTelemetry.preciseSleepCalls),
	            static_cast<unsigned long long>(framePacingTelemetry.coarseSleepCalls),
	            static_cast<unsigned long long>(framePacingTelemetry.spinPolls),
	            static_cast<unsigned long long>(framePacingTelemetry.fpsDisplaySamples),
	            fpsDisplayAverageFrameMs,
	            fpsDisplayMaxFrameMs,
	            framePacingTelemetry.lastFpsDisplayFrameMs,
	            framePacingTelemetry.lastFpsDisplayValue,
	            framePacingTelemetry.minFpsDisplayValue,
	            framePacingTelemetry.maxFpsDisplayValue);
}

void ONScripter::drawFpsOverlay() {
	if (!fps_overlay_visible || !screen_target)
		return;

	constexpr uint16_t overlayWidth  = 170;
	constexpr uint16_t overlayHeight = 44;
	constexpr float overlayX         = 16.0f;
	constexpr float overlayY         = 16.0f;

	if (fps_overlay_text.empty())
		fps_overlay_text = "FPS: --.-";

	if (!fps_overlay_gpu || fps_overlay_gpu->w != overlayWidth || fps_overlay_gpu->h != overlayHeight) {
		if (fps_overlay_gpu)
			gpu.freeImage(fps_overlay_gpu);
		fps_overlay_gpu = gpu.createImage(overlayWidth, overlayHeight, 4);
		GPU_GetTarget(fps_overlay_gpu);
		fps_overlay_dirty = true;
	}

	if (fps_overlay_dirty) {
		auto *target = GPU_GetTarget(fps_overlay_gpu);
		SDL_Color background{0, 0, 0, 180};
		GPU_Rect backgroundRect{0, 0, static_cast<float>(overlayWidth), static_cast<float>(overlayHeight)};
		GPU_RectangleFilled2(target, backgroundRect, background);

		Fontinfo font = sentence_font;
		font.reset();
		font.top_xy[0]   = 10;
		font.top_xy[1]   = 7;
		font.borderPadding = 2;

		auto &style              = font.changeStyle();
		style.color              = {0xff, 0xff, 0xff};
		style.is_gradient        = false;
		style.is_centered        = false;
		style.is_fitted          = false;
		style.is_bold            = true;
		style.is_italic          = false;
		style.is_shadow          = false;
		style.shadow_distance[0] = 0;
		style.shadow_distance[1] = 0;
		style.is_border          = true;
		style.border_width       = 40;
		style.border_color       = {0, 0, 0};
		style.font_size          = 24;
		style.character_spacing = 0;
		style.line_height        = 30;
		style.wrap_limit         = overlayWidth - 20;

		RenderRect clip{0, 0, static_cast<float>(overlayWidth), static_cast<float>(overlayHeight)};
		dlgCtrl.renderToTarget(target, &clip, const_cast<char *>(fps_overlay_text.c_str()), &font, false, FIT_MODE::FIT_BOTH);
		fps_overlay_dirty = false;
	}

	gpu.copyGPUImage(fps_overlay_gpu, nullptr, nullptr, screen_target, overlayX, overlayY);
	screenChanged = true;
}

void ONScripter::waitEvent(int count, bool nopPreferred) {
	//sendToLog(LogLevel::Info, "----waitEventSub(%i)\n", count);
	static unsigned int lastExitTime   = 0;
	unsigned int externalTimeThreshold = 5; // for instance

	unsigned int thisCallTime = SDL_GetTicks();
	if (!(skip_mode & SKIP_SUPERSKIP) && lastExitTime) {
		if (nopPreferred && thisCallTime - lastExitTime < externalTimeThreshold) {
			return;
		}
	}

	bool timerBreakout = count >= 0;
	//TODO: remove me, when done with constant_refresh
	static int nested_calls = 0;
	nested_calls++;

	if (nested_calls != 1) {
		errorAndExit("You are completely mad to use SDL_Events like that");
	}

	static float active_fps_rate       = 0.0f;
	static unsigned int last_fps_probe = 0;
	static FPSTimeGenerator *fps_timer = nullptr;
	static uint64_t accumulatedOvershootNanos = 0;
	static uint64_t lastFlipTimeNanos         = 0;
	static uint64_t frameTailEstimateNanos    = 0;

	if (!fps_timer || thisCallTime - last_fps_probe >= 1000) {
		float detected_fps = effectiveRefreshRate();
		float fps_diff = detected_fps - active_fps_rate;
		if (fps_diff < 0.0f)
			fps_diff = -fps_diff;
		if (!fps_timer || fps_diff > 0.01f) {
			delete fps_timer;
			fps_timer       = new FPSTimeGenerator(detected_fps);
			active_fps_rate = detected_fps;
			accumulatedOvershootNanos = 0;
			lastFlipTimeNanos         = 0;
			frameTailEstimateNanos    = 0;
		}
		last_fps_probe = thisCallTime;
	}
	auto fps = fps_timer;

	unsigned int ticks = thisCallTime; // SDL_GetTicks()
	uint64_t thisCallTimeNanos = highResolutionTicksNanos();
	bool resetFramePacing      = lastFlipTimeNanos == 0 || thisCallTimeNanos - lastFlipTimeNanos > STALE_FRAME_BASELINE_NS;

	do {
		uint64_t framesOvershoot = 0;
		uint64_t nanosPerFrame   = fps->nanosPerFrame();
		uint64_t timeThisFrame{nanosPerFrame};
		uint64_t waitThisFrame{timeThisFrame};
		const bool collectFramePacingTelemetry = framePacingTelemetryEnabled();
		const uint64_t maxTailCompensation = std::min(timeThisFrame / 2, MAX_FRAME_TAIL_COMPENSATION_NS);
		const uint64_t tailCompensation    = std::min(frameTailEstimateNanos, maxTailCompensation);
		if (tailCompensation < waitThisFrame)
			waitThisFrame -= tailCompensation;
		if (collectFramePacingTelemetry) {
			framePacingTelemetry.targetFrameNanos = timeThisFrame;
			framePacingTelemetry.waitTargetNanos  = waitThisFrame;
		}
		if (resetFramePacing) {
			accumulatedOvershootNanos = 0;
			lastFlipTimeNanos         = thisCallTimeNanos;
			frameTailEstimateNanos    = 0;
			resetFramePacing          = false;
		}
		while (accumulatedOvershootNanos > timeThisFrame) {
			// must skip this frame :(
			accumulatedOvershootNanos -= timeThisFrame;
			framesOvershoot++;
		}

		advanceGameState(nanosPerFrame * (framesOvershoot + 1)); // may advance multiple frames if we are lagging
		if (save_load_overlay_active) {
			stepSaveLoadOverlay();
		} else if (allow_rendering) {
			constantRefresh();
		}
		handleSDLEvents();
		joyCtrl.handleUsbEvents();
		mainThreadDowntimeProcessing(true); // we must unfortunately call it at least once (and don't care whether it did anything, ignore return value)

		if (request_video_shutdown) {
			auto vidLayer = getLayer<MediaLayer>(video_layer);
			if (vidLayer && vidLayer->isPlaying(true)) {
				if (vidLayer->stopPlayback(MediaLayer::FinishMode::Normal)) {
					request_video_shutdown = false;
				}
			} else {
				request_video_shutdown = false;
			}
		}

		if (allow_rendering && !save_load_overlay_active && !(skip_mode & SKIP_SUPERSKIP) && !deferredLoadingEnabled) {
			if ((fps_overlay_visible || fps_overlay_refresh_required) && !screenChanged) {
				RenderRect fullRect = full_script_clip;
				flushDirect(fullRect, fullRect, refreshMode() | CONSTANT_REFRESH_MODE | REFRESH_BEFORESCENE_MODE);
			}

			if (cursor_gpu) {
				int x, y;
				onsGetMouseState(&x, &y);
				gpu.copyGPUImage(cursor_gpu, nullptr, nullptr, screen_target, x, y);
			}

			drawFpsOverlay();

			GPU_FlushBlitBuffer();
		}

		if (cursorAutoHide && lastCursorMove + 5000 < ticksNow) {
			cursorState(false);
		}

		while (true) {
			ticksNow = SDL_GetTicks();
			uint64_t ticksNowNanos = highResolutionTicksNanos();
			uint64_t frameElapsed  = ticksNowNanos - lastFlipTimeNanos;
			if (frameElapsed >= waitThisFrame) {
				if (frameElapsed > timeThisFrame) {
					accumulatedOvershootNanos += frameElapsed - timeThisFrame;
					if (collectFramePacingTelemetry) {
						++framePacingTelemetry.overshootFrames;
						framePacingTelemetry.overshootNanos += frameElapsed - timeThisFrame;
					}
				}
				break;
			}
			// We don't want to be precise in SSKIP mode
			if (skip_mode & SKIP_SUPERSKIP)
				break;
			// we still have time, do some downtime processing
			bool processed{mainThreadDowntimeProcessing(false)};
			if (collectFramePacingTelemetry) {
				++framePacingTelemetry.downtimePolls;
				if (processed)
					++framePacingTelemetry.downtimeWorkPolls;
			}
			if (!processed) {
				const uint64_t remainingNanos = waitThisFrame - frameElapsed;
#if defined(ONS_USE_SDL3)
				if (remainingNanos > PRECISE_SLEEP_MIN_NS + PRECISE_SLEEP_GUARD_NS) {
					if (collectFramePacingTelemetry)
						++framePacingTelemetry.preciseSleepCalls;
					SDL_DelayPrecise(remainingNanos - PRECISE_SLEEP_GUARD_NS);
					continue;
				}
#else
				if (timeThisFrame >= COARSE_SLEEP_FRAME_MIN_NS &&
				    remainingNanos >= COARSE_SLEEP_REMAINING_MIN_NS) {
					if (collectFramePacingTelemetry)
						++framePacingTelemetry.coarseSleepCalls;
					SDL_Delay(1);
					continue;
				}
#endif
				if (collectFramePacingTelemetry)
					++framePacingTelemetry.spinPolls;
			}
		}

		const uint64_t frameTailStartNanos = highResolutionTicksNanos();

#if defined(DROID)
		// A resize event is not the only way the canvas and the surface can end
		// up disagreeing. changeMode() derives the fullscreen geometry from
		// display metrics when it runs, and at startup -- launching while the
		// display is still settling, for instance -- those can describe an
		// orientation the window never had, with no resize to react to. The
		// renderer notices the mismatch directly and it is corrected here.
		if (GPU_TakeSurfaceGeometryStale()) {
			window.applySurfaceGeometry();
			droidResumeRedraw.store(true, std::memory_order_release);
		}

		if (droidResumeRedraw.exchange(false, std::memory_order_acq_rel)) {
			// Android handed back a fresh surface. The scene itself did not
			// change, so without forcing one frame nothing would ever be
			// presented: the screen stays black while audio keeps playing.
			allow_rendering = true;
			markRetainedRainSceneStaticDirty();
			before_dirty_rect_scene.fill(window.canvas_width, window.canvas_height);
			screenChanged = true;
		}
#endif

		if (allow_rendering && !save_load_overlay_active && !(skip_mode & SKIP_SUPERSKIP) && !deferredLoadingEnabled) {
			if (cursor)
				SDL_SetCursor(nullptr);
			if (screenChanged && !window.getFullscreenFix() && should_flip) {
				GPU_Flip(screen_target);
				screenChanged = false;
				gpu.clearWholeTarget(screen_target);
				fps_overlay_refresh_required = false;
			} else {
				// We didn't update, assume screenChanged to be false
				screenChanged = false;
			}
		}

#ifndef DROID
		// We still must invoke this on many platforms to prevent "not responding" issues.
		// On droid it is not necessary and it additionally breaks background app handling in Android_PumpEvents
		SDL_PollEvent(nullptr);
#endif

		//sendToLog(LogLevel::Info,"  flipped -- aimed for %i ms, took %i ms\n", constant_refresh_interval, ticksNow - lastFlipTime);
		const uint64_t frameEndNanos = highResolutionTicksNanos();
		const uint64_t frameNanos    = frameEndNanos >= lastFlipTimeNanos ? frameEndNanos - lastFlipTimeNanos : 0;
		if (collectFramePacingTelemetry && frameEndNanos >= lastFlipTimeNanos) {
			++framePacingTelemetry.frames;
			framePacingTelemetry.totalFrameNanos += frameNanos;
			framePacingTelemetry.maxFrameNanos = std::max(framePacingTelemetry.maxFrameNanos, frameNanos);
		}

		if ((show_fps_counter || fps_overlay_visible) && !(skip_mode & SKIP_SUPERSKIP) && frameNanos > 0) {
			static std::deque<double> ticksList;
			static uint64_t lastTitleUpdateNanos = 0;
			// Display FPS from the same completed-frame interval used by pacing telemetry.
			double elapsedMillis = static_cast<double>(frameNanos) / NANOS_PER_MILLISECOND;
			ticksList.push_front(elapsedMillis);
			if (ticksList.size() > 30)
				ticksList.pop_back();
			double av = std::accumulate(ticksList.begin(), ticksList.end(), 0.0) / ticksList.size();
			if (collectFramePacingTelemetry) {
				++framePacingTelemetry.fpsDisplaySamples;
				framePacingTelemetry.fpsDisplayTotalFrameNanos += frameNanos;
				framePacingTelemetry.fpsDisplayMaxFrameNanos = std::max(framePacingTelemetry.fpsDisplayMaxFrameNanos, frameNanos);
				framePacingTelemetry.lastFpsDisplayFrameMs = av;
				framePacingTelemetry.lastFpsDisplayValue   = 1000.0 / av;
				if (framePacingTelemetry.minFpsDisplayValue == 0.0 ||
				    framePacingTelemetry.lastFpsDisplayValue < framePacingTelemetry.minFpsDisplayValue)
					framePacingTelemetry.minFpsDisplayValue = framePacingTelemetry.lastFpsDisplayValue;
				framePacingTelemetry.maxFpsDisplayValue = std::max(framePacingTelemetry.maxFpsDisplayValue,
				                                                   framePacingTelemetry.lastFpsDisplayValue);
			}
			if (fps_overlay_visible)
				updateFpsCounter(av);
			if (show_fps_counter && (lastTitleUpdateNanos == 0 || frameEndNanos - lastTitleUpdateNanos >= FPS_DISPLAY_UPDATE_INTERVAL_NS)) {
				char titlestring[512];
				std::snprintf(titlestring, sizeof(titlestring), "[Renderer: %s / TPF: %.3f ms / FPS: %.3f] %s%s",
				              gpu.current_renderer->name, av, 1000.0 / av, volume_on_flag ? "" : "[Sound: Off] ", wm_title_string);
				window.setTitle(titlestring);
				lastTitleUpdateNanos = frameEndNanos;
			}
		}
		if (!(skip_mode & SKIP_SUPERSKIP) && frameEndNanos >= frameTailStartNanos) {
			const uint64_t frameTailNanos = std::min(frameEndNanos - frameTailStartNanos, maxTailCompensation);
			if (frameTailEstimateNanos == 0)
				frameTailEstimateNanos = frameTailNanos;
			else
				frameTailEstimateNanos = (frameTailEstimateNanos * 7 + frameTailNanos) / 8;
		}
		lastFlipTimeNanos = frameEndNanos;

		//printClock("(next iteration)");

		if (!endOfEventBatch) {
			// we were broken out prematurely by some condition we were waiting for, so we should return.
			if (count > 0) {
				dynamicProperties.advance(count); // advance the time we skipped
				dynamicProperties.apply();
			}
			break;
		}

		count -= (ticksNow - ticks);
		ticks = ticksNow;
		//sendToLog(LogLevel::Info,"  next iteration\n");
	} while (count > 0 || !timerBreakout); // if we are told not to break out by timer, this is an infinite loop
	//sendToLog(LogLevel::Info, "-----------------\n");
	nested_calls--;

	lastExitTime = SDL_GetTicks();

	// New process --
	// ConstantUpdate
	// ConstantRefresh
	// Process current events (if we are provided with a timer don't leave the function after flipping, instead return to step 1)
	// Wait until n ms since last flip (interleave loading). Or -- do some loading, then wait for vsync (if we decide to use that?)
	// Flip
	// Return -- here we assume that we are not going to be gone long but that isn't respected atm
	// need to check in during label execute and this fn return back if we've not been gone long
}

void ONScripter::trapHandler() {
	// End video if we are allowed to skip
	if (video_skip_mode == VideoSkip::Normal) {
		request_video_shutdown = true;
		// Script is responsible for handling trap-based exits
	} else if (video_skip_mode == VideoSkip::Trap) {
		video_skip_mode = VideoSkip::NotPlaying;
	}

	stopCursorAnimation(clickstr_state);
	setCurrentLabel(lrTrap.dest);
	lrTrap = LRTrap();
}

/* **************************************** *
 * Event handlers
 * **************************************** */
bool ONScripter::beginScrollableScrollbarDrag(int x, int y) {
	for (int i = MAX_SPRITE_NUM - 1; i >= 0; --i) {
		AnimationInfo &ai                 = sprite_info[i];
		AnimationInfo::ScrollableInfo &si = ai.scrollableInfo;
		if (!ai.visible || !ai.exists || !si.isSpecialScrollable || !si.respondsToMouseOver || !si.scrollbar)
			continue;
		if (si.totalHeight <= ai.scrollable.h || si.scrollbarHeight <= 0)
			continue;

		AnimationInfo *scrollbar = si.scrollbar;
		if (!scrollbar->visible || !scrollbar->exists)
			continue;
		if (x < scrollbar->pos.x || x >= scrollbar->pos.x + scrollbar->pos.w ||
		    y < scrollbar->pos.y || y >= scrollbar->pos.y + scrollbar->pos.h)
			continue;

		scrollbarDragState.scrollableSprite = i;
		scrollbarDragState.grabOffsetY      = y - scrollbar->pos.y;
		return true;
	}

	return false;
}

bool ONScripter::updateScrollableScrollbarDrag(int x, int y) {
	if (!scrollbarDragState.active())
		return false;

	if (scrollbarDragState.scrollableSprite < 0 || scrollbarDragState.scrollableSprite >= MAX_SPRITE_NUM) {
		endScrollableScrollbarDrag();
		return false;
	}

	AnimationInfo &ai                 = sprite_info[scrollbarDragState.scrollableSprite];
	AnimationInfo::ScrollableInfo &si = ai.scrollableInfo;
	if (!ai.visible || !ai.exists || !si.isSpecialScrollable || !si.scrollbar ||
	    si.totalHeight <= ai.scrollable.h || si.scrollbarHeight <= 0) {
		endScrollableScrollbarDrag();
		return false;
	}

	const int thumbY    = std::clamp(y - scrollbarDragState.grabOffsetY, si.scrollbarTop, si.scrollbarTop + si.scrollbarHeight);
	const int maxScroll = si.totalHeight - ai.scrollable.h;
	const int targetY   = ((thumbY - si.scrollbarTop) * maxScroll + si.scrollbarHeight / 2) / si.scrollbarHeight;

	dynamicProperties.addSpriteProperty(&ai, scrollbarDragState.scrollableSprite, false, true,
	                                    SPRITE_PROPERTY_SCROLLABLE_Y, targetY, 0, MOTION_EQUATION_LINEAR, true);
	si.snapType = AnimationInfo::ScrollSnap::NONE;
	mouseOverCheck(x, y, true);
	flush(refreshMode());
	return true;
}

void ONScripter::endScrollableScrollbarDrag() {
	scrollbarDragState = ScrollbarDragState();
}

bool ONScripter::mouseMoveEvent(SDL_MouseMotionEvent &event, EventProcessingState &state) {
	controlMode = ControlMode::Mouse;

	state.buttonState.x = event.x;
	state.buttonState.y = event.y;
	window.translateWindowToScriptCoords(state.buttonState.x, state.buttonState.y);

	if (event_mode & WAIT_BUTTON_MODE) {
		if (updateScrollableScrollbarDrag(state.buttonState.x, state.buttonState.y))
			return false;

		mouseOverCheck(state.buttonState.x, state.buttonState.y);
		if (getmouseover_flag && hoveringButton &&
		    (hoveredButtonNumber >= getmouseover_min) &&
		    (hoveredButtonNumber <= getmouseover_max)) {
			// Both NScripter and ONScripter do not distinguish mouse over from a click.
			// This is nonsense, so we add a magic value large enough (10000) to do so.
			// Since the buttons are normally expected to be within 1~999 range and negative ones are
			// usually reserved for hardware keys, this sounds like a reasonable solution.
			state.buttonState.set(10000 + hoveredButtonNumber);
			playClickVoice();
			stopCursorAnimation(clickstr_state);
			return true;
		}
		if (btnarea_flag &&
		    (((btnarea_pos < 0) && (event.y > -btnarea_pos)) ||
		     ((btnarea_pos > 0) && (event.y < btnarea_pos)))) {
			state.buttonState.set(-4);
			playClickVoice();
			stopCursorAnimation(clickstr_state);
			return true;
		}
	}
	return false;
}

bool ONScripter::mouseButtonDecision(EventProcessingState &state, bool left, bool right, bool middle, bool up, bool down) {
	auto rclick = [&](EventProcessingState &state) {
		if ((rmode_flag && (event_mode & WAIT_TEXT_MODE)) ||
		    (event_mode & (WAIT_BUTTON_MODE | WAIT_RCLICK_MODE))) {
			state.buttonState.set(-1);
			for (int i = 0; i < MAX_SPRITE_NUM; ++i) {
				AnimationInfo *ai = &sprite_info[i];
				if (!ai->exists)
					continue;
				if (ai->scrollableInfo.isSpecialScrollable && ai->scrollableInfo.respondsToClick && ai->scrollableInfo.mouseCursorIsOverHoveredElement) {
					state.buttonState.set(-81);
					break;
				}
			}
			return true;
		}
		return false;
	};

	auto lclick = [&](EventProcessingState &state, bool down) {
		if (hoveringButton) {
			state.buttonState.set(hoveredButtonNumber);
		} else {
			state.buttonState.set(0);
			for (int i = 0; i < MAX_SPRITE_NUM; ++i) {
				AnimationInfo *ai = &sprite_info[i];
				if (!ai->exists)
					continue;
				if (ai->scrollableInfo.isSpecialScrollable && ai->scrollableInfo.respondsToClick && ai->scrollableInfo.mouseCursorIsOverHoveredElement) {
					state.buttonState.set(-80);
					break;
				}
			}
		}
		if (event_mode & WAIT_TEXTOUT_MODE && skip_enabled) {
			state.skipMode |= (SKIP_TO_WAIT | SKIP_TO_EOL);
			// script cannot detect _TO_WAIT or _TO_EOL using isskip etc -- at best TO_EOP page,
			// so from script POV this is not a change in its state, so, no eventCallbackRequired here
		}
		skip_effect = true;
		if (video_skip_mode == VideoSkip::Normal) {
			request_video_shutdown = true;
		}
		if (down)
			state.buttonState.down_flag = true;

		if (state.buttonState.valid_flag && (event_mode & WAIT_INPUT_MODE) && WaitVoiceAction::isCurrent(state.handler)) {
			currentAction(state.handler)->terminate();
		}

		return true;
	};

	auto mclick = [&](EventProcessingState &state, bool down) {
		if (!getmclick_flag)
			return false;
		state.buttonState.set(-70);
		if (down)
			state.buttonState.down_flag = true;
		return true;
	};

	return (right && up && rclick(state)) || //right-click
	       (left && lclick(state, down)) ||  //left-click
	       (middle && mclick(state, down));  //middle-click
}

bool ONScripter::checkClearAutomode(EventProcessingState &state, bool up) {
	//any mousepress clears automode, on the release
	if (up) {
		sendToLog(LogLevel::Info, "automode cleared by input\n");
		addToPostponedEventChanges([this]() { eventCallbackRequired = true; automode_flag = false; });
		if (getskipoff_flag && (event_mode & WAIT_BUTTON_MODE)) {
			state.buttonState.set(-61);
			return true;
		}
	}
	return false;
}

bool ONScripter::checkClearTrap(bool left, bool right) {
	if (lrTrap.enabled) {
		//trap that mouseclick!
		if ((right && lrTrap.right) || (left && lrTrap.left)) {

			addToPostponedEventChanges("trapHandler", [this]() { trapHandler(); });

			/* This one might have returned us during waitCommand, so it needs to signal now as well */
			if (event_mode & WAIT_WAIT_MODE) {
				for (const auto &a : fetchConstantRefreshActions<WaitAction>()) a->terminate();
			}
			if (event_mode & WAIT_DELAY_MODE) {
				for (const auto &a : fetchConstantRefreshActions<DelayAction>()) a->terminate();
			}

			return true;
		}
	}
	return false;
}

bool ONScripter::checkClearSkip(EventProcessingState &state) {
	if (getskipoff_flag && (state.skipMode & SKIP_NORMAL) && (event_mode & WAIT_BUTTON_MODE)) {
		eventCallbackRequired = true;
		state.skipMode &= ~SKIP_NORMAL;
		state.buttonState.set(-60);
		return true;
	}

	if (state.skipMode & SKIP_NORMAL)
		eventCallbackRequired = true;
	state.skipMode &= ~SKIP_NORMAL;
	return false;
}

bool ONScripter::checkClearVoice() {
	if (event_mode & (WAIT_INPUT_MODE | WAIT_BUTTON_MODE)) {
		addToPostponedEventChanges("play click voice", [this] { playClickVoice(); });
		addToPostponedEventChanges([this] { stopCursorAnimation(clickstr_state); });
		if (event_mode & WAIT_DELAY_MODE) {
			for (const auto &a : fetchConstantRefreshActions<DelayAction>()) a->terminate();
		}
		return true;
	}

	return false;
}

// returns true if should break out of the event loop
bool ONScripter::mousePressEvent(SDL_MouseButtonEvent &event, EventProcessingState &state) {
	if (event_mode & WAIT_BUTTON_MODE)
		last_keypress = SDL_NUM_SCANCODES;

	bool type_up    = event.type == SDL_MOUSEBUTTONUP;
	bool type_down  = event.type == SDL_MOUSEBUTTONDOWN;
	bool btn_left   = event.button == SDL_BUTTON_LEFT;
	bool btn_right  = event.button == SDL_BUTTON_RIGHT;
	bool btn_middle = event.button == SDL_BUTTON_MIDDLE;

	if (btn_left && type_up && scrollbarDragState.active()) {
		endScrollableScrollbarDrag();
		return false;
	}

	if (automode_flag)
		return checkClearAutomode(state, type_up);

	if (checkClearTrap(btn_left, btn_right))
		return true;

	state.buttonState.reset();
	state.buttonState.x = event.x;
	state.buttonState.y = event.y;
	window.translateWindowToScriptCoords(state.buttonState.x, state.buttonState.y);
	state.buttonState.down_flag = false;

	if (checkClearSkip(state))
		return true;

	if ((event_mode & WAIT_BUTTON_MODE) && btn_left) {
		if (type_down && beginScrollableScrollbarDrag(state.buttonState.x, state.buttonState.y))
			return false;
	}

	if (!mouseButtonDecision(state, btn_left, btn_right, btn_middle, type_up, type_down))
		return false;

	return checkClearVoice();
}

bool ONScripter::touchEvent(SDL_Event &event, EventProcessingState &state) {
	if (event_mode & WAIT_BUTTON_MODE)
		last_keypress = SDL_NUM_SCANCODES;

	bool btn_left   = false;
	bool btn_right  = false;
	bool btn_middle = false;
	bool type_up    = event.type == SDL_FINGERUP || event.type == ONS_MULTIGESTURE_EVENT;
	bool type_down  = event.type == SDL_FINGERDOWN;
	float event_x = 0, event_y = 0;

	if (event.type == ONS_MULTIGESTURE_EVENT) {
#if defined(ONS_USE_SDL3)
		return false;
#else
		auto sendKeyEvent = [this](SDL_Scancode c) {
			SDL_Event k{};
			onsKeyboardScancode(k.key) = c;
			k.type                     = SDL_KEYUP;
			localEventQueue.emplace_front(k);
		};

		SDL_MultiGestureEvent &gesture = event.mgesture;

		//sendToLog(LogLevel::Error, "Multiguesture %d last %d, num %d (%f, %f)\n",
		//			event.common.timestamp, last_touchswipe_time,
		//			event.mgesture.numFingers, event.mgesture.dDist, event.mgesture.dTheta);

		// New movement
		if (last_touchswipe_time + MAX_TOUCH_SWIPE_TIMESPAN < gesture.timestamp) {
			last_touchswipe.x = gesture.x;
			last_touchswipe.y = gesture.y;
			last_touchswipe.w = last_touchswipe.h = 0;
			last_touchswipe_time                  = gesture.timestamp;
		}

		// We are applying the action, ignore the rest of the swipe
		if (last_touchswipe_time <= gesture.timestamp) {
			if (gesture.numFingers == 2) {
				SDL_MouseWheelEvent wheel{};
				wheel.type = SDL_MOUSEWHEEL;
				wheel.x    = 0;
				wheel.y    = (last_touchswipe.y - gesture.y) * touch_scroll_mul;
				return mouseScrollEvent(wheel, state);
			}
			if (gesture.numFingers == 3) {
				last_touchswipe.w = gesture.x - last_touchswipe.x; // w > 0 -> right
				last_touchswipe.h = gesture.y - last_touchswipe.y; // h > 0 -> down

				if (last_touchswipe.w > TOUCH_ACTION_THRESHOLD_X) { // right
					sendKeyEvent(ONS_SCANCODE_SKIP);
				} else if (last_touchswipe.w < -TOUCH_ACTION_THRESHOLD_X) { // left
					sendKeyEvent(SDL_SCANCODE_A);
				} else if (last_touchswipe.h > TOUCH_ACTION_THRESHOLD_Y) { // down
					sendKeyEvent(SDL_SCANCODE_TAB);
				} else if (last_touchswipe.h < -TOUCH_ACTION_THRESHOLD_Y) { // up
					sendKeyEvent(ONS_SCANCODE_MUTE);
				} else {
					return false;
				}

				// Ignore later events for some time
				last_touchswipe_time = gesture.timestamp + MAX_TOUCH_SWIPE_TIMESPAN;
			}
		}
		return false;
#endif
	}

	//sendToLog(LogLevel::Error, "Finger prevention %d %d (num %d)\n",
	//			last_touchswipe_time, event.tfinger.timestamp, onsTouchFingerId(event.tfinger));

	// Prevent extra clicks right after scrolling
	if (last_touchswipe_time + MAX_TOUCH_SWIPE_TIMESPAN >= event.tfinger.timestamp)
		return false;

	// fingerId contains grouped finger amount after tapping
	if (onsTouchFingerId(event.tfinger) == 1)
		btn_left = true;
	else if (onsTouchFingerId(event.tfinger) == 2)
		btn_right = true;
	else
		btn_middle = true;

	event_x = static_cast<int>(event.tfinger.x * window.script_width);
	event_y = static_cast<int>(event.tfinger.y * window.script_height);

	if (automode_flag)
		return checkClearAutomode(state, type_up);

	if (checkClearTrap(btn_left, btn_right))
		return true;

	state.buttonState.reset();
	state.buttonState.x         = event_x;
	state.buttonState.y         = event_y;
	state.buttonState.down_flag = false;

	if (checkClearSkip(state))
		return true;

	if (!mouseButtonDecision(state, btn_left, btn_right, btn_middle, type_up, type_down))
		return false;

	return checkClearVoice();
}

bool ONScripter::mouseScrollEvent(SDL_MouseWheelEvent &event, EventProcessingState &state) {
	last_wheelscroll = event.y;

	addToPostponedEventChanges("scroll scrollables", [this]() {
		auto scrollSprites = [&](AnimationInfo *sprites) {
			for (int i = 0; i < MAX_SPRITE_NUM; ++i) {
				AnimationInfo *scrollElem = &sprites[i];
				if (!scrollElem->exists)
					continue;
				if (scrollElem->scrollable.h > 0 && scrollElem->scrollableInfo.respondsToMouseOver) {
					dynamicProperties.addSpriteProperty(scrollElem, scrollElem->id, scrollElem->type == SPRITE_LSP2, false,
					                                    SPRITE_PROPERTY_SCROLLABLE_Y, mouse_scroll_mul * last_wheelscroll, 100, 1, true);
					scrollElem->scrollableInfo.snapType = AnimationInfo::ScrollSnap::NONE;
				}
			}
		};
		scrollSprites(sprite_info);
		scrollSprites(sprite2_info);
	});

	if (event.y > 0 &&
	    ((event_mode & WAIT_TEXT_MODE) ||
	     (usewheel_flag && (event_mode & WAIT_BUTTON_MODE)))) {
		state.buttonState.set(-2);
	} else if (event.y < 0 &&
	           ((enable_wheeldown_advance_flag && (event_mode & WAIT_TEXT_MODE)) ||
	            (usewheel_flag && (event_mode & WAIT_BUTTON_MODE)))) {
		state.buttonState.set((event_mode & WAIT_TEXT_MODE) ? 0 : -3);
	} else
		return false;

	return checkClearVoice();
}

void ONScripter::shiftHoveredButtonInDirection(int diff) {
	// If we are in this function, our buttons are valid, and a valid default is set.
	int totalButtonCount           = getTotalButtonCount();
	ONScripter::ButtonLink *button = root_button_link.next;

	// If the last known hovered button number is nowhere to be found, then we need to set the link index to the default (0 unless declared with btnhover_d).
	if (buttonNumberToLinkIndex(lastKnownHoveredButtonNumber) == -1) {
		lastKnownHoveredButtonLinkIndex = buttonNumberToLinkIndex(hoveredButtonDefaultNumber);
	}

	auto newLinkIndex = lastKnownHoveredButtonLinkIndex;
	newLinkIndex += diff;
	if (newLinkIndex < 0)
		newLinkIndex = totalButtonCount - 1;
	else if (newLinkIndex >= totalButtonCount)
		newLinkIndex = 0;

	for (int i = 0; i < newLinkIndex; ++i) {
		button = button->next;
	}

	if (button) {
		// Trigger the same code that mouseOverCheck triggers on button hover.
		controlMode = ControlMode::Arrow;
		doHoverButton(true, button->no, newLinkIndex, button);
	}
}

int ONScripter::buttonNumberToLinkIndex(int buttonNo) {
	int totalButtons = getTotalButtonCount();
	ButtonLink *button{root_button_link.next};
	for (int i = 0; i < totalButtons; i++) {
		if (!button) {
			return -1;
		}
		if (button->no == buttonNo) {
			return i;
		}
		button = button->next;
	}
	return -1;
}

int ONScripter::getTotalButtonCount() const {
	int totalButtonCount = 0;
	ButtonLink *button   = root_button_link.next;
	while (button) {
		button = button->next;
		++totalButtonCount;
	}
	return totalButtonCount;
}

// returns true if should break out of the event loop
bool ONScripter::keyDownEvent(SDL_KeyboardEvent &event, EventProcessingState &state) {
	if (event_mode & WAIT_BUTTON_MODE)
		last_keypress = onsKeyboardScancode(event);

	int last_ctrl_status = state.keyState.ctrl;

	// keyState.ctrl assignment can't be completely deferred due to caller requiring it; must at least pass the updates to caller
	switch (onsKeyboardScancode(event)) {
#ifdef MACOSX
		case SDL_SCANCODE_LGUI:
		case SDL_SCANCODE_RGUI:
			if (!ons_cfg_options.contains("skip-on-cmd"))
				break;
			if (onsKeyboardScancode(event) == SDL_SCANCODE_LGUI || onsKeyboardScancode(event) == SDL_SCANCODE_RGUI) {
				state.keyState.apple |= 1;
				onsKeyboardScancode(event) = SDL_SCANCODE_LCTRL;
			}
#endif
		case SDL_SCANCODE_RCTRL:
		case SDL_SCANCODE_LCTRL:
			if (onsKeyboardScancode(event) == SDL_SCANCODE_LCTRL || onsKeyboardScancode(event) == SDL_SCANCODE_RCTRL)
				if (skipIsAllowed()) {
					state.keyState.ctrl |= (onsKeyboardScancode(event) == SDL_SCANCODE_LCTRL ? 0x02 : 0x01);
					internal_slowdown_counter = 0; //maybe a slightly wrong place to do it
				}
			if (!skipIsAllowed())
				break; //Skip not allowed, exit
			if (last_ctrl_status != state.keyState.ctrl) {
				skip_effect = true; // allow short-circuiting the current effect with ctrl
				if (video_skip_mode == VideoSkip::Normal) {
					request_video_shutdown = true;
				}
			}
			//Ctrl key: do skip in text
			if (event_mode & (WAIT_INPUT_MODE | WAIT_TEXTOUT_MODE | WAIT_TEXTBTN_MODE)) {
				state.buttonState.set(0);

				if (event_mode & WAIT_WAIT_MODE) {
					for (const auto &a : fetchConstantRefreshActions<WaitAction>()) a->terminate();
				}
				if (event_mode & WAIT_DELAY_MODE) {
					for (const auto &a : fetchConstantRefreshActions<DelayAction>()) a->terminate();
				}

				addToPostponedEventChanges("play click voice", [this]() { playClickVoice(); });
				stopCursorAnimation(clickstr_state);
				return true;
			}
			if (event_mode & (WAIT_SLEEP_MODE)) {
				stopCursorAnimation(clickstr_state);
				return true;
			}
			break;
		case SDL_SCANCODE_RALT:
			state.keyState.opt |= 0x01;
			break;
		case SDL_SCANCODE_LALT:
			state.keyState.opt |= 0x02;
			break;
		case SDL_SCANCODE_RSHIFT:
			state.keyState.shift |= 0x01;
			break;
		case SDL_SCANCODE_LSHIFT:
			state.keyState.shift |= 0x02;
			break;
		default:
			break;
	}

	return false;
}

void ONScripter::keyUpEvent(SDL_KeyboardEvent &event, EventProcessingState &state) {
	if (event_mode & WAIT_BUTTON_MODE)
		last_keypress = onsKeyboardScancode(event);

	switch (onsKeyboardScancode(event)) {
#ifdef MACOSX
		case SDL_SCANCODE_LGUI:
		case SDL_SCANCODE_RGUI:
			if (!ons_cfg_options.contains("skip-on-cmd"))
				break;
			state.keyState.apple &= ~1;
#endif
		case SDL_SCANCODE_RCTRL:
			state.keyState.ctrl &= ~0x01;
			break;
		case SDL_SCANCODE_LCTRL:
			state.keyState.ctrl &= ~0x02;
			break;
		case SDL_SCANCODE_RALT:
			state.keyState.opt &= ~0x01;
			break;
		case SDL_SCANCODE_LALT:
			state.keyState.opt &= ~0x02;
			break;
		case SDL_SCANCODE_RSHIFT:
			state.keyState.shift &= ~0x01;
			break;
		case SDL_SCANCODE_LSHIFT:
			state.keyState.shift &= ~0x02;
			break;
		default:
			break;
	}
}

// returns true if should break out of the event loop
bool ONScripter::keyPressEvent(SDL_KeyboardEvent &event, EventProcessingState &state) {
	//reset the button state
	state.buttonState.reset();
	state.buttonState.down_flag = false;

	if (automode_flag)
		return checkClearAutomode(state, event.type == SDL_KEYUP);

	if (event.type == SDL_KEYUP) {
		//'m' is for mute (toggle)
		if (((onsKeyboardScancode(event) == SDL_SCANCODE_M && state.keyState.opt) ||
		     onsKeyboardScancode(event) == ONS_SCANCODE_MUTE) &&
		    !state.keyState.ctrl) {
			addToPostponedEventChanges("setVolumeMute", [this]() {
				if (!script_mute) {
					volume_on_flag = !volume_on_flag;
					setVolumeMute(!volume_on_flag);
					sendToLog(LogLevel::Info, "turned %s volume mute\n", !volume_on_flag ? "on" : "off");
				} else {
					sendToLog(LogLevel::Info, "disallowed atm");
				}
			});
		}

		if ((onsKeyboardScancode(event) == SDL_SCANCODE_E && state.keyState.opt) ||
		    onsKeyboardScancode(event) == ONS_SCANCODE_SCREEN) {
			needs_screenshot = true;
		}

#if !defined(IOS) && !defined(DROID)
		if (onsKeyboardScancode(event) == SDL_SCANCODE_F && state.keyState.opt && !state.keyState.ctrl) {
			addToPostponedEventChanges("toggle fps overlay", [this]() { toggleFpsOverlay(); });
			state.keyState.pressedFlag = true;
			return false;
		}
#endif
	}

	// 's', Return, Enter, or Space will clear (regular) skip mode
	// Yes, just 's' without the modifiers to make it easier.
	if ((event.type == SDL_KEYUP) &&
	    (onsKeyboardScancode(event) == SDL_SCANCODE_RETURN ||
	     onsKeyboardScancode(event) == SDL_SCANCODE_KP_ENTER ||
	     onsKeyboardScancode(event) == SDL_SCANCODE_SPACE ||
	     onsKeyboardScancode(event) == SDL_SCANCODE_S ||
	     onsKeyboardScancode(event) == ONS_SCANCODE_SKIP)) {
		if (checkClearSkip(state))
			return true;
	}

	// i to spew some debug information
	/*if (event.type == SDL_KEYUP && onsKeyboardScancode(event) == SDL_SCANCODE_i) {
		sendToLog(LogLevel::Error, "Last executed command lines:\n");
		for (auto &log : script_h.debugCommandLog)
			sendToLog(LogLevel::Error, "%s\n", log.c_str());
	}*/

	if (checkClearTrap((onsKeyboardScancode(event) == SDL_SCANCODE_RETURN ||
	                    onsKeyboardScancode(event) == SDL_SCANCODE_KP_ENTER ||
	                    onsKeyboardScancode(event) == SDL_SCANCODE_SPACE),
	                   onsKeyboardScancode(event) == SDL_SCANCODE_ESCAPE))
		return true;

	//so many ways to 'left-click' a button
	if ((event_mode & WAIT_BUTTON_MODE) &&
	    (((event.type == SDL_KEYUP || btndown_flag) &&
	      ((!getenter_flag && onsKeyboardScancode(event) == SDL_SCANCODE_RETURN) ||
	       (!getenter_flag && onsKeyboardScancode(event) == SDL_SCANCODE_KP_ENTER))) ||
	     ((spclclk_flag || !useescspc_flag) &&
	      onsKeyboardScancode(event) == SDL_SCANCODE_SPACE))) {
		if (onsKeyboardScancode(event) == SDL_SCANCODE_RETURN ||
		    onsKeyboardScancode(event) == SDL_SCANCODE_KP_ENTER ||
		    (spclclk_flag && onsKeyboardScancode(event) == SDL_SCANCODE_SPACE)) {
			state.buttonState.set(hoveringButton ? hoveredButtonNumber : 0);
			if (event.type == SDL_KEYDOWN)
				state.buttonState.down_flag = true;
		} else {
			state.buttonState.set(0);
		}
		skip_effect = true;
		if (video_skip_mode == VideoSkip::Normal) {
			request_video_shutdown = true;
		}

		if (event_mode & WAIT_DELAY_MODE) {
			for (const auto &a : fetchConstantRefreshActions<DelayAction>()) a->terminate();
		}

		addToPostponedEventChanges("play click voice", [this]() { playClickVoice(); });
		stopCursorAnimation(clickstr_state);
		return true;
	}

	if (event.type == SDL_KEYDOWN)
		return false;

	if ((event_mode & (WAIT_INPUT_MODE | WAIT_BUTTON_MODE)) &&
	    (autoclick_time == 0 || (event_mode & WAIT_BUTTON_MODE))) {
		//Esc is for 'right-click' (sometimes)
		if (!useescspc_flag && onsKeyboardScancode(event) == SDL_SCANCODE_ESCAPE) {
			state.buttonState.set(-1);
		} else if (useescspc_flag && onsKeyboardScancode(event) == SDL_SCANCODE_ESCAPE) {
			state.buttonState.set(-10);
		} else if (!spclclk_flag && useescspc_flag && onsKeyboardScancode(event) == SDL_SCANCODE_SPACE) {
			state.buttonState.set(-11);
		}
		//'h' or left-arrow for page-up
		else if (((!getcursor_flag && onsKeyboardScancode(event) == SDL_SCANCODE_LEFT) ||
		          onsKeyboardScancode(event) == SDL_SCANCODE_H) &&
		         ((event_mode & WAIT_TEXT_MODE) ||
		          (usewheel_flag && !getcursor_flag &&
		           (event_mode & WAIT_BUTTON_MODE)))) {
			state.buttonState.set(-2);
		}
		//'l' or right-arrow for page-down
		else if (((!getcursor_flag && onsKeyboardScancode(event) == SDL_SCANCODE_RIGHT) ||
		          onsKeyboardScancode(event) == SDL_SCANCODE_L) &&
		         ((enable_wheeldown_advance_flag &&
		           (event_mode & WAIT_TEXT_MODE)) ||
		          (usewheel_flag && (event_mode & WAIT_BUTTON_MODE)))) {
			if (event_mode & WAIT_TEXT_MODE) {
				state.buttonState.set(0);
			} else {
				state.buttonState.set(-3);
			}
		}
		//'k', 'p', or up-arrow for shift to mouseover next button
		else if (((!getcursor_flag && onsKeyboardScancode(event) == SDL_SCANCODE_UP) ||
		          onsKeyboardScancode(event) == SDL_SCANCODE_K ||
		          onsKeyboardScancode(event) == SDL_SCANCODE_P) &&
		         (event_mode & WAIT_BUTTON_MODE)) {
			addToPostponedEventChanges("shiftHoveredButtonInDirection", [this]() {
				shiftHoveredButtonInDirection(1);
			});
			return false;
		}
		//'j', 'n', or down-arrow for shift to mouseover previous button
		else if (((!getcursor_flag && onsKeyboardScancode(event) == SDL_SCANCODE_DOWN) ||
		          onsKeyboardScancode(event) == SDL_SCANCODE_J ||
		          onsKeyboardScancode(event) == SDL_SCANCODE_N) &&
		         (event_mode & WAIT_BUTTON_MODE)) {
			addToPostponedEventChanges("shiftHoveredButtonInDirection", [this]() {
				shiftHoveredButtonInDirection(-1);
			});
			return false;
		} else if (getcursor_flag && (onsKeyboardScancode(event) == SDL_SCANCODE_UP || onsKeyboardScancode(event) == SDL_SCANCODE_DOWN || onsKeyboardScancode(event) == SDL_SCANCODE_LEFT || onsKeyboardScancode(event) == SDL_SCANCODE_RIGHT) &&
		           ((enable_wheeldown_advance_flag && (event_mode & WAIT_TEXT_MODE)) ||
		            (usewheel_flag && (event_mode & WAIT_BUTTON_MODE)))) {
			addToPostponedEventChanges("change scrollable hovered element", [this, event]() {
				Direction d = getDirection(onsKeyboardScancode(event));
				auto shiftSprites = [&](AnimationInfo *sprites) {
					for (int i = 0; i < MAX_SPRITE_NUM; ++i) {
						AnimationInfo *sptr = &sprites[i];
						if (sptr->visible && sptr->exists && sptr->scrollableInfo.isSpecialScrollable)
							changeScrollableHoveredElement(sptr, d);
					}
				};
				shiftSprites(sprite_info);
				shiftSprites(sprite2_info);
			});
		} else if (getpageup_flag && (onsKeyboardScancode(event) == SDL_SCANCODE_PAGEUP)) {
			state.buttonState.set(-12);
		} else if (getpagedown_flag && (onsKeyboardScancode(event) == SDL_SCANCODE_PAGEDOWN)) {
			state.buttonState.set(-13);
		} else if ((getenter_flag && (onsKeyboardScancode(event) == SDL_SCANCODE_RETURN)) ||
		           (getenter_flag && (onsKeyboardScancode(event) == SDL_SCANCODE_KP_ENTER))) {
			state.buttonState.set(-19);
		} else if (gettab_flag && (onsKeyboardScancode(event) == SDL_SCANCODE_TAB)) {
			state.buttonState.set(-20);
		} else if (getcursor_flag && (onsKeyboardScancode(event) == SDL_SCANCODE_UP)) {
			state.buttonState.set(-40);
		} else if (getcursor_flag && (onsKeyboardScancode(event) == SDL_SCANCODE_RIGHT)) {
			state.buttonState.set(-41);
		} else if (getcursor_flag && (onsKeyboardScancode(event) == SDL_SCANCODE_DOWN)) {
			state.buttonState.set(-42);
		} else if (getcursor_flag && (onsKeyboardScancode(event) == SDL_SCANCODE_LEFT)) {
			state.buttonState.set(-43);
		} else if (getinsert_flag && (onsKeyboardScancode(event) == SDL_SCANCODE_INSERT)) {
			state.buttonState.set(-50);
		} else if (getzxc_flag && (onsKeyboardScancode(event) == SDL_SCANCODE_Z)) {
			state.buttonState.set(-51);
		} else if (getzxc_flag && (onsKeyboardScancode(event) == SDL_SCANCODE_X)) {
			state.buttonState.set(-52);
		} else if (getzxc_flag && (onsKeyboardScancode(event) == SDL_SCANCODE_C)) {
			state.buttonState.set(-53);
		} else if (getfunction_flag) {
			if (onsKeyboardScancode(event) == SDL_SCANCODE_F1)
				state.buttonState.set(-21);
			else if (onsKeyboardScancode(event) == SDL_SCANCODE_F2)
				state.buttonState.set(-22);
			else if (onsKeyboardScancode(event) == SDL_SCANCODE_F3)
				state.buttonState.set(-23);
			else if (onsKeyboardScancode(event) == SDL_SCANCODE_F4)
				state.buttonState.set(-24);
			else if (onsKeyboardScancode(event) == SDL_SCANCODE_F5)
				state.buttonState.set(-25);
			else if (onsKeyboardScancode(event) == SDL_SCANCODE_F6)
				state.buttonState.set(-26);
			else if (onsKeyboardScancode(event) == SDL_SCANCODE_F7)
				state.buttonState.set(-27);
			else if (onsKeyboardScancode(event) == SDL_SCANCODE_F8)
				state.buttonState.set(-28);
			else if (onsKeyboardScancode(event) == SDL_SCANCODE_F9)
				state.buttonState.set(-29);
			else if (onsKeyboardScancode(event) == SDL_SCANCODE_F10)
				state.buttonState.set(-30);
			else if (onsKeyboardScancode(event) == SDL_SCANCODE_F11)
				state.buttonState.set(-31);
			else if (onsKeyboardScancode(event) == SDL_SCANCODE_F12)
				state.buttonState.set(-32);
		}
		if (state.buttonState.valid_flag) {
			stopCursorAnimation(clickstr_state);
			return true;
		}
	};

	//catch 'left-button click' that fell through?
	if ((event_mode & WAIT_INPUT_MODE) && !state.keyState.pressedFlag &&
	    (autoclick_time == 0 || (event_mode & WAIT_BUTTON_MODE))) {
		//check for "button click"
		if (onsKeyboardScancode(event) == SDL_SCANCODE_RETURN ||
		    onsKeyboardScancode(event) == SDL_SCANCODE_KP_ENTER ||
		    onsKeyboardScancode(event) == SDL_SCANCODE_SPACE) {
			state.keyState.pressedFlag = true;
			skip_effect                = true;
			if (video_skip_mode == VideoSkip::Normal) {
				request_video_shutdown = true;
			}
			state.buttonState.set(0);

			if (event_mode & WAIT_DELAY_MODE) {
				for (const auto &a : fetchConstantRefreshActions<DelayAction>()) a->terminate();
			}

			addToPostponedEventChanges("play click voice", [this]() { playClickVoice(); });
			stopCursorAnimation(clickstr_state);

			return true;
		}
	}

	if ((event_mode & (WAIT_INPUT_MODE | WAIT_TEXTBTN_MODE | WAIT_TEXTOUT_MODE)) &&
	    !state.keyState.pressedFlag) {
		//'s' is for skip mode
		if (((onsKeyboardScancode(event) == SDL_SCANCODE_S && state.keyState.opt) || onsKeyboardScancode(event) == ONS_SCANCODE_SKIP) &&
		    !automode_flag && !state.keyState.ctrl && skipIsAllowed()) {
			if (!(state.skipMode & SKIP_NORMAL))
				skip_effect = true; // short-circuit a current effect
			state.skipMode |= SKIP_NORMAL;
			internal_slowdown_counter = 0; //maybe a slightly wrong place to do it
			                               //if (onsKeyboardScancode(event) == SDL_SCANCODE_D) state.skipMode |= SKIP_SUPERSKIP; // rocket engines engaged
			//sendToLog(LogLevel::Info, "toggle skip to true\n");
			state.keyState.pressedFlag = true;
			if (video_skip_mode == VideoSkip::Normal) {
				request_video_shutdown = true;
			}
			state.buttonState.set(0);

			if (event_mode & WAIT_WAIT_MODE) {
				for (const auto &a : fetchConstantRefreshActions<WaitAction>()) a->terminate();
			}
			if (event_mode & WAIT_DELAY_MODE) {
				for (const auto &a : fetchConstantRefreshActions<DelayAction>()) a->terminate();
			}

			stopCursorAnimation(clickstr_state);

			return true;
		}
		//'a' is for automode (gamepad L1 maps to this scancode as well)
		if (onsKeyboardScancode(event) == SDL_SCANCODE_A &&
		    !state.keyState.ctrl && mode_ext_flag && !automode_flag) {
			addToPostponedEventChanges("change to automode", [this]() { eventCallbackRequired = true; automode_flag = true; });
			state.skipMode &= ~SKIP_NORMAL;
			sendToLog(LogLevel::Info, "change to automode\n");
			state.keyState.pressedFlag = true;
			state.buttonState.set(0);
			stopCursorAnimation(clickstr_state);

			return true;
		}
	}

#if !defined(IOS) && !defined(DROID)
	//'f' is for fullscreen toggle
	if (onsKeyboardScancode(event) == SDL_SCANCODE_F && !state.keyState.ctrl && !state.keyState.opt) {
		addToPostponedEventChanges("change window mode", []() {
			window.changeMode(true, false, !window.getFullscreen());
		});
	}
#endif

	//using insani's skippable wait
	if ((event_mode & WAIT_SLEEP_MODE) && (onsKeyboardScancode(event) == SDL_SCANCODE_S || onsKeyboardScancode(event) == ONS_SCANCODE_SKIP) && skipIsAllowed()) {
		state.skipMode |= SKIP_TO_WAIT;
		state.skipMode &= ~SKIP_NORMAL;
		state.keyState.pressedFlag = true;
	}
	if ((state.skipMode & SKIP_TO_WAIT) &&
	    (onsKeyboardScancode(event) == SDL_SCANCODE_RETURN ||
	     onsKeyboardScancode(event) == SDL_SCANCODE_KP_ENTER ||
	     onsKeyboardScancode(event) == SDL_SCANCODE_SPACE)) {
		state.skipMode &= ~SKIP_TO_WAIT;
		state.keyState.pressedFlag = true;
	}
	if ((event_mode & WAIT_TEXTOUT_MODE) && skipIsAllowed() &&
	    (onsKeyboardScancode(event) == SDL_SCANCODE_RETURN ||
	     onsKeyboardScancode(event) == SDL_SCANCODE_KP_ENTER ||
	     onsKeyboardScancode(event) == SDL_SCANCODE_SPACE)) {
		state.skipMode |= (SKIP_TO_WAIT | SKIP_TO_EOL);
		state.keyState.pressedFlag = true;
	}

	if ((onsKeyboardScancode(event) == SDL_SCANCODE_F1) && (version_str != nullptr)) {
		//F1 is for Help (on Windows), so show the About dialog box
		addToPostponedEventChanges("display message box", [this]() {
			window.showSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "About", version_str);
		});

		state.keyState.pressedFlag = true;
	}

	return false;
}

void ONScripter::translateKeyDownEvent(SDL_Event &event, EventProcessingState &state, bool &ret, bool ctrl_toggle) {
	if (state.skipMode & SKIP_SUPERSKIP)
		return;
	if (event.key.type == SDL_JOYBUTTONDOWN) {
		// Translate before mutating: this event object is dispatched once per
		// handler, and rewriting the type first turns an ignored joystick
		// duplicate into a phantom keyboard event for the remaining handlers.
		SDL_Scancode scancode = joyCtrl.transButton(event.jbutton.button, event.jbutton.which);
		if (scancode == SDL_SCANCODE_UNKNOWN)
			return;
		event.key.type            = SDL_KEYDOWN;
		onsKeyboardScancode(event.key) = scancode;
	}

	ret                  = keyDownEvent(event.key, state);
	bool new_ctrl_toggle = ctrl_toggle ^ (state.keyState.ctrl != 0);
	//allow skipping sleep waits with start of ctrl keydown
	ret |= (event_mode & WAIT_SLEEP_MODE) && new_ctrl_toggle;
	if (btndown_flag)
		ret |= keyPressEvent(event.key, state);
	addToPostponedEventChanges([this, state]() {
		keyState             = state.keyState;
		current_button_state = state.buttonState;
		skip_mode            = state.skipMode;
	});
	if (skip_mode != state.skipMode || keyState.ctrl != state.keyState.ctrl)
		eventCallbackRequired = true;
}

void ONScripter::translateKeyUpEvent(SDL_Event &event, EventProcessingState &state, bool &ret) {
	if (state.skipMode & SKIP_SUPERSKIP)
		return;
	if (event.key.type == SDL_JOYBUTTONUP) {
		// Translate before mutating: this event object is dispatched once per
		// handler, and rewriting the type first turns an ignored joystick
		// duplicate into a phantom keyboard event for the remaining handlers.
		SDL_Scancode scancode = joyCtrl.transButton(event.jbutton.button, event.jbutton.which);
		if (scancode == SDL_SCANCODE_UNKNOWN)
			return;
		event.key.type            = SDL_KEYUP;
		onsKeyboardScancode(event.key) = scancode;
	} else if (event.key.type == SDL_JOYHATMOTION) {
		SDL_Scancode scancode = joyCtrl.transHat(event.jhat.value, event.jhat.which);
		if (scancode == SDL_SCANCODE_UNKNOWN)
			return;
		event.key.type            = SDL_KEYUP;
		onsKeyboardScancode(event.key) = scancode;
	}

	keyUpEvent(event.key, state);
	ret = keyPressEvent(event.key, state);
	addToPostponedEventChanges([this, state]() {
		keyState             = state.keyState;
		current_button_state = state.buttonState;
		skip_mode            = state.skipMode;
	});
	if (skip_mode != state.skipMode || keyState.ctrl != state.keyState.ctrl)
		eventCallbackRequired = true;
}

bool ONScripter::mainThreadDowntimeProcessing(bool essentialProcessingOnly) {

	bool didSomething{false};

	// Load chunk call
	// Check loadGPUImageByChunks if you want to use this.
	/*if (imageLoader.isActive && !imageLoader.isLoaded) {
		imageLoader.loadChunk();
		didSomething = true;
	}*/

	if (allow_rendering && !essentialProcessingOnly) {
		didSomething |= gpu.handleScheduledJobs();
	}

	return didSomething;
}

void ONScripter::handleRegisteredActions(uint64_t ns) {
	Lock lock(&ons.registeredCRActions);
	auto action = registeredCRActions.begin();
	while (action != registeredCRActions.end()) {
		std::shared_ptr<ConstantRefreshAction> a = *action;
		a->advance(ns);
		if (a->terminated || a->expired()) {
			a->onExpired();
			action = registeredCRActions.erase(action);
			continue;
		} else {
			a->run();
		}
		++action;
	}
}

void ONScripter::advanceGameState(uint64_t ns) {
	current_game_state_advance_nanos = ns;

	serviceDiscordPresence();

	handleRegisteredActions(ns);
	camera.update(static_cast<unsigned int>(ns / 1000000));

	// update animation clocks
	advanceAIclocks(ns);

	// should we make this a function?
	for (auto &ss : spritesets) {
		if (ss.second.warpAmplitude != 0) {
			ss.second.warpClock.tickNanos(ns);
			fillCanvas(true, true);
			flush(refreshMode());
		}
	}

	if (warpAmplitude != 0) {
		warpClock.tickNanos(ns);
		fillCanvas(true, true);
		flush(refreshMode());
	}

	dlgCtrl.advanceDialogueRendering(ns);

	dynamicProperties.advanceNanos(ns);
	dynamicProperties.apply();
}

void ONScripter::constantRefresh() {

	if (proceedAnimation() >= 0) {
		if (!before_dirty_rect_scene.isEmpty() || !before_dirty_rect_hud.isEmpty() || camera.has_moved) {
			flush(refreshMode() |
			          (draw_cursor_flag ? REFRESH_CURSOR_MODE : 0) |
			          REFRESH_BEFORESCENE_MODE,
			      &before_dirty_rect_scene.bounding_box_script,
			      &before_dirty_rect_hud.bounding_box_script,
			      false, true);
		}
	}

	bool effectIsOver = false;
	if (effect_current) {
		if (!effect_set) {
			bool terminateEffect = setEffect();
			if (terminateEffect)
				effect_current = nullptr;
			else {
				effect_set = true;
				if (effectskip_flag) {
					if (!skip_enabled)
						event_mode |= WAIT_INPUT_MODE;
					skip_effect = false;
				}
			}
		}
	}
	if (effect_current) {
		if (effectskip_flag && skip_effect && skip_enabled) {
			effect_counter = effect_duration;
			fillCanvas();
		}
		effectIsOver = !doEffect();
		/*sendToLog(LogLevel::Info, "effect_current: %p, effect_set: %i, effectIsOver: %i, pre_screen_render %i, constant_refresh_mode %i\n", effect_current, effect_set, effectIsOver, pre_screen_render, constant_refresh_mode);*/
	}

	RenderRect *hud_rect, *scene_rect;

	if (effectIsOver) {
		hud_rect = scene_rect = nullptr;
	} else if (!effect_current) {
		hud_rect   = &before_dirty_rect_hud.bounding_box_script;
		scene_rect = &before_dirty_rect_scene.bounding_box_script;
	} else {
		// ... do we actually use these rects in the case of effect_current?
		hud_rect   = &dirty_rect_hud.bounding_box_script;
		scene_rect = &dirty_rect_scene.bounding_box_script;
	}

	if (effect_current) {
		if (!pre_screen_render && !effectIsOver)
			errorAndExit("Neither pre_screen_render nor effectIsOver are set during the effect");
		// It is OK to pass refresh modes in here while effect is ongoing, because pre_screen_render should be set here, therefore, nothing new will be created
		// In fact, even REFRESH_BEFORESCENE_MODE is not needed until last_call
		flush(CONSTANT_REFRESH_MODE | REFRESH_BEFORESCENE_MODE, scene_rect, hud_rect, effect_rect_cleanup, false);
	} else if (display_mode & DISPLAY_MODE_TEXT) {
		//When we are in DISPLAY_MODE_TEXT (and normal mode) we don't clear our rects.
		//This is incorrect (due to animations/quakes) for cr. Make sure we at least have this part in CR
		addTextWindowClip(before_dirty_rect_hud);
		//Our CR mode is always resetted due to specific style of CR.
		//alphaBlendText gives proper hud_gpu to us, but we (may) update it with our cursors
		if (constant_refresh_mode != REFRESH_NONE_MODE)
			constant_refresh_mode |= (REFRESH_TEXT_MODE | REFRESH_WINDOW_MODE);
		flush(constant_refresh_mode | CONSTANT_REFRESH_MODE | REFRESH_BEFORESCENE_MODE, scene_rect, hud_rect, true, false);
	} else {
		flush(constant_refresh_mode | CONSTANT_REFRESH_MODE | REFRESH_BEFORESCENE_MODE, scene_rect, hud_rect, true, false);
	}

	if (effectIsOver) {
		effect_current = nullptr;
		event_mode &= ~(WAIT_INPUT_MODE);
	}

	constant_refresh_mode     = REFRESH_NONE_MODE;
	constant_refresh_executed = true;
}

ONScripter::EventProcessingState::EventProcessingState(unsigned int _handler) {
	keyState    = ons.keyState;
	buttonState = ons.current_button_state;
	skipMode    = ons.skip_mode;
	eventMode   = ons.event_mode;
	handler     = _handler;
}

void ONScripter::runEventLoop() {
	Lock lock(&ons.registeredCRActions);

	SDL_Event eventStorage{};
	SDL_Event *event = &eventStorage;
	bool started_in_automode = automode_flag;

	while (true) {
		*event = localEventQueue.back();
		localEventQueue.pop_back();

		endOfEventBatch = false;

		if (exitCode.load(std::memory_order_relaxed) != ExitType::None) {
			ons.requestQuit(exitCode);
			return; //dummy
		}

		bool ret{false};
		bool ctrl_toggle{keyState.ctrl != 0};
		bool chunk_reported_return{false};

		bool mouseMotionHandlingDone{false};
		int defaultEventMode{event_mode};

		for (unsigned int handler = 0; handler <= registeredCRActions.size(); handler++) {
			ret = false;
			if (handler == registeredCRActions.size()) {
				event_mode = defaultEventMode;
				if (isWaitingForUserInput() || isWaitingForUserInterrupt()) {
					if (isInputEvent(event->type)) {
						// There should be more, I think
						assert(!((event_mode & WAIT_BUTTON_MODE) || ((event_mode & WAIT_INPUT_MODE) && !effect_current)));
						continue;
					}
				}
			} else {
				const auto &action = registeredCRActions[handler];
				event_mode         = action->eventMode();
				if (!action->handlesEvent(event->type)) {
					// this event type is not handled by this handler
					continue;
				}
			}

			// Handle event with this event_mode
			{
				EventProcessingState state(handler);

				switch (event->type) {
					case SDL_MOUSEMOTION:
						if (mouseMotionHandlingDone)
							break;
						mouseMotionHandlingDone = ret = mouseMoveEvent(event->motion, state);
						addToPostponedEventChanges([this, state]() {
							current_button_state = state.buttonState;
							if (cursorAutoHide) {
								lastCursorMove = ticksNow;
								cursorState(true);
							}
						});
						break;

#if defined(IOS) || defined(DROID)
					case ONS_MULTIGESTURE_EVENT:
#if !defined(ONS_USE_SDL3)
						// Such a thing called crapdroid sends erratic move events on move attempts
						// with a distance of less than 0.00X smth. Here we try to ignore them to some level,
						// since we use gesture events to protect us from accidental r-click (double-tap) during
						/// the scrolling.
						if (std::fabs(event->mgesture.dDist) < 0.01 && std::fabs(event->mgesture.dTheta) < 0.01)
							break;
#endif
					case SDL_FINGERDOWN:
						if (event->type == SDL_FINGERDOWN && !btndown_flag)
							break;
					case SDL_FINGERUP:
						if (state.skipMode & SKIP_SUPERSKIP)
							break;
						ret = touchEvent(*event, state);
						addToPostponedEventChanges([this, state]() { current_button_state = state.buttonState; skip_mode = state.skipMode; });
						break;
#else

					case SDL_MOUSEBUTTONDOWN:
						if (state.skipMode & SKIP_SUPERSKIP)
							break;
						if ((event_mode & WAIT_BUTTON_MODE) && event->button.button == SDL_BUTTON_LEFT) {
							int x = event->button.x;
							int y = event->button.y;
							window.translateWindowToScriptCoords(x, y);
							if (beginScrollableScrollbarDrag(x, y))
								break;
						}
						if (!btndown_flag)
							break;
						/* fall through */
					case SDL_MOUSEBUTTONUP:
						if (state.skipMode & SKIP_SUPERSKIP)
							break;
						ret = mousePressEvent(event->button, state);
						addToPostponedEventChanges([this, state]() { current_button_state = state.buttonState; skip_mode = state.skipMode; });
						break;

					case SDL_MOUSEWHEEL:
						ret = mouseScrollEvent(event->wheel, state);
						addToPostponedEventChanges([this, state]() { current_button_state = state.buttonState; });
						break;
#endif

					case SDL_JOYDEVICEADDED:
						joyCtrl.handleDeviceAdded(event->jdevice.which);
						ret = true;
						break;

					case SDL_JOYDEVICEREMOVED:
						joyCtrl.handleDeviceRemoved(event->jdevice.which);
						ret = true;
						break;

#if ONS_USE_SDL3
					case SDL_EVENT_GAMEPAD_ADDED:
						joyCtrl.handleDeviceAdded(event->gdevice.which);
						ret = true;
						break;

					case SDL_EVENT_GAMEPAD_REMOVED:
						joyCtrl.handleDeviceRemoved(event->gdevice.which);
						ret = true;
						break;

					case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
						SDL_Event keyEvent{};
						keyEvent.type     = SDL_KEYDOWN;
						keyEvent.key.type = SDL_KEYDOWN;
						onsKeyboardScancode(keyEvent.key) = joyCtrl.transGamepadButton(event->gbutton.button, event->gbutton.which);
						if (onsKeyboardScancode(keyEvent.key) != SDL_SCANCODE_UNKNOWN)
							translateKeyDownEvent(keyEvent, state, ret, ctrl_toggle);
						break;
					}

					case SDL_EVENT_GAMEPAD_BUTTON_UP: {
						SDL_Event keyEvent{};
						keyEvent.type     = SDL_KEYUP;
						keyEvent.key.type = SDL_KEYUP;
						onsKeyboardScancode(keyEvent.key) = joyCtrl.transGamepadButton(event->gbutton.button, event->gbutton.which);
						if (onsKeyboardScancode(keyEvent.key) != SDL_SCANCODE_UNKNOWN)
							translateKeyUpEvent(keyEvent, state, ret);
						break;
					}

					case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
#if !defined(IOS) && !defined(DROID)
						auto ke = joyCtrl.transGamepadAxis(event->gaxis);
						if (onsKeyboardScancode(ke.key) != SDL_SCANCODE_UNKNOWN) {
							if (ke.type == SDL_KEYDOWN)
								translateKeyDownEvent(ke, state, ret, ctrl_toggle);
							else
								translateKeyUpEvent(ke, state, ret);
						}
#endif
						break;
					}
#endif

					case SDL_JOYBUTTONDOWN:
					case SDL_KEYDOWN:
						translateKeyDownEvent(*event, state, ret, ctrl_toggle);
						break;

					case SDL_JOYHATMOTION:
					case SDL_JOYBUTTONUP:
					case SDL_KEYUP:
						translateKeyUpEvent(*event, state, ret);
						break;

					case SDL_JOYAXISMOTION: {
#if !defined(IOS) && !defined(DROID)
						auto ke = joyCtrl.transAxis(event->jaxis);
						if (onsKeyboardScancode(ke.key) != SDL_SCANCODE_UNKNOWN) {
							if (ke.type == SDL_KEYDOWN)
								translateKeyDownEvent(ke, state, ret, ctrl_toggle);
							else
								translateKeyUpEvent(ke, state, ret);
						}
#endif
						break;
					}

					case ONS_EVENT_BATCH_END:
						endOfEventBatch = true;
						ret             = true;
						break;

					case ONS_CHUNK_EVENT:
						flushEventSub(*event);
						//sendToLog(LogLevel::Info, "ONS_CHUNK_EVENT %d: %x %d %x\n", event.user.code, wave_sample[0], automode_flag, event_mode);
						if (event->user.code != 0 || !(event_mode & WAIT_VOICE_MODE))
							break;
						event_mode &= ~WAIT_VOICE_MODE;

						chunk_reported_return = true;
						// Falls through -- will return from waitEvent (prematurely) after doing a final UPKEEP

					case ONS_UPKEEP_EVENT:
						if ((event_mode & WAIT_VOICE_MODE) && wave_sample[0] && Mix_Playing(0) && !Mix_Paused(0)) {
							break;
						}

						if (!automode_flag && started_in_automode && clickstr_state != CLICK_NONE) {
							started_in_automode = false;
							break;
						}

						if ((event_mode & (WAIT_INPUT_MODE | WAIT_BUTTON_MODE)) &&
						    (clickstr_state == CLICK_WAIT || clickstr_state == CLICK_NEWPAGE)) {
							playClickVoice();
							stopCursorAnimation(clickstr_state);
						}
						ret = chunk_reported_return;
						break; //will return right after this event in ONS_EVENT_BATCH_END, possibly breaks a call from fade event

#if defined(IOS) || defined(DROID)
					case SDL_APP_WILLENTERBACKGROUND:
						// This gets called when the user hits the home button, or gets a call.
						window.setActiveState(false);
						allow_rendering = false;
						sendToLog(LogLevel::Info, "Entering background\n");
						break;
					case SDL_APP_DIDENTERBACKGROUND:
						sendToLog(LogLevel::Info, "Entered background\n");
						break;
					case SDL_APP_WILLENTERFOREGROUND:
						sendToLog(LogLevel::Info, "Leaving background\n");
						break;
					case SDL_APP_DIDENTERFOREGROUND:
						// Your app is interactive and getting CPU again.
						window.setActiveState(true);
						allow_rendering = true;
						markRetainedRainSceneStaticDirty();
						before_dirty_rect_scene.fill(window.canvas_width, window.canvas_height);
						sendToLog(LogLevel::Info, "Left background\n");
						break;
					case SDL_APP_LOWMEMORY:
						sendToLog(LogLevel::Info, "Received low memory warning\n");
						break;
#endif

					case SDL_USEREVENT:
						if (event->user.code == ONS_MUSIC_EVENT ||
						    event->user.code == ONS_SEQMUSIC_EVENT)
							flushEventSub(*event);
						break;

#if defined(ONS_USE_SDL3)
					case SDL_WINDOWEVENT_RESTORED:
					case SDL_WINDOWEVENT_MAXIMIZED:
					case SDL_WINDOWEVENT_RESIZED:
					case SDL_WINDOWEVENT_EXPOSED:
					case SDL_WINDOWEVENT_MOVED:
#else
					case SDL_WINDOWEVENT:
#endif
#if defined(DROID)
						// Android changes the surface under a live activity, so
						// this is the only notice the engine gets that its
						// fullscreen geometry is now for the wrong orientation.
						if (onsWindowEventType(event->window) == SDL_WINDOWEVENT_RESIZED) {
							window.applySurfaceGeometry();
							markRetainedRainSceneStaticDirty();
							before_dirty_rect_scene.fill(window.canvas_width, window.canvas_height);
							// Nothing in the scene changed, so without asking
							// for one the engine would not flip and the stale
							// frame would stay on screen.
							droidResumeRedraw.store(true, std::memory_order_release);
						}
#endif
#ifdef MACOSX
						// OS X specific: We are done exiting fullscreen mode and the animation has finished
						if (onsWindowEventType(event->window) == SDL_WINDOWEVENT_RESTORED && window.getFullscreenFix() && !window.getFullscreen()) {
							if (window.changeMode(false, true))
								fillCanvas(true, true);
						}
						// OS X specific: We are done entering fullscreen mode and the animation has finished
						// Note: this may fail to do its work, if the latter block is not present, but we are guranteed to get
						// SDL_WINDOWEVENT_MAXIMIZED as a last event in entering fullscreen, so we need it to disable getFullscreenFix()
						else if (onsWindowEventType(event->window) == SDL_WINDOWEVENT_MAXIMIZED && window.getFullscreenFix() && window.getFullscreen()) {
							if (window.changeMode(false, true))
								fillCanvas(true, true);
							// OS X specific: We are entering/leaving fullscreen mode and window resizing is in progress
						} else if (onsWindowEventType(event->window) == SDL_WINDOWEVENT_RESIZED) { // Fired by SDL when backing scale factor changes
							addToPostponedEventChanges("backing scale factor changed", []() {
								if (window.changeMode(false, true, window.getFullscreen()))
									ons.fillCanvas(true, true);
							});
						}
#else
						// At least Windows and Linux want us to act on SDL_WINDOWEVENT_EXPOSED
						if (onsWindowEventType(event->window) == SDL_WINDOWEVENT_EXPOSED && window.getFullscreenFix()) {
							if (window.changeMode(false, true))
								fillCanvas(true, true);
						}
#endif
						// At least Linux specific: We are showing some window part that was hidden before
						else if (onsWindowEventType(event->window) == SDL_WINDOWEVENT_EXPOSED || onsWindowEventType(event->window) == SDL_WINDOWEVENT_MOVED) {
							// Now that we have commands like textoff2 we are not allowed to recklessly update hud
							markRetainedRainSceneStaticDirty();
							before_dirty_rect_scene.fill(window.canvas_width, window.canvas_height);
							//fillCanvas(false, true);
						}

						break;
					case SDL_QUIT:
						endCommand();
						break;
					default:
						break;
				}

				// WARNING: These may be in an improper place, particularly buttonWaitAction.
				// If you intend to respond to a click, put it in mousePressEvent, etc.
				if (handler < registeredCRActions.size()) {
					const auto &action = registeredCRActions[handler];
					auto *bma          = dynamic_cast<ButtonMonitorAction *>(action.get());
					if (bma) {
						if (state.buttonState.valid_flag)
							bma->buttonState = state.buttonState;
					}
					auto *bwa = dynamic_cast<ButtonWaitAction *>(action.get());
					if (bwa) {
						if (state.buttonState.valid_flag) {
							// Regardless of wait-for-voice or not, buttons should always terminate a ButtonWaitAction. (Unless it's async.)
							bwa->buttonState = state.buttonState;
							action->terminate();
						} else if (bwa->eventMode() & WAIT_VOICE_MODE && !(bwa->eventMode() & WAIT_TIMER_MODE) && !bwa->timer_set) {
							// This is a wait-for-voice.
							// When the voice ends, we are expected to expire the wait, or otherwise set up a timer that will expire it later.
							// (If we already went through this code once to set up a timer, then we don't need to do this, of course.)
							if (!(wave_sample[0] && Mix_Playing(0) && !Mix_Paused(0))) {
								// The voice has ended.
								// Is there an additional delay that we're supposed to wait for?
								int32_t additionalWaitTime{0};
								if (!ignore_voicedelay) {
									if (bwa->voiced_txtbtnwait && voicedelay_time)
										additionalWaitTime = voicedelay_time;
									if (bwa->final_voiced_txtbtnwait && final_voicedelay_time)
										additionalWaitTime = final_voicedelay_time;
								}
								// If there's no delay, this will expire immediately. (Same as terminate.)
								bwa->clock.setCountdown(additionalWaitTime);
								bwa->timer_set = true;
							}
						}
					}
				}
			}
		}

		// Execute all postponed changes
		for (auto &f : postponedEventChanges) f();
		postponedEventChanges.clear();
		postponedEventChangeLabels.clear();

		// Only return based on the final default handler
		if (ret)
			return;
	}
}
