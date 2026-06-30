/**
 *  ObjectFall.cpp
 *  ONScripter-RU
 *
 *  "snow.dll" analogue with improved dencity and performance.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Layers/ObjectFall.hpp"
#include "Engine/Core/ONScripter.hpp"
#include "Engine/Graphics/GPU.hpp"

#include "Engine/Graphics/RendererBackend.hpp"

#include <cmath>
#include <cstring>
#include <limits>
#include <random>

ObjectFallLayer::ObjectFallLayer(uint32_t w, uint32_t h)
    : Layer(w, h) {
	collectTelemetry = telemetryEnabled();
	baseDrop = gpu.createImage(baseDropWidth, baseDropHeight, 4);
	GPU_GetTarget(baseDrop);
	gpu.clear(baseDrop->target, baseDropColour.r, baseDropColour.g, baseDropColour.b, baseDropColour.a);
	gpu.multiplyAlpha(baseDrop);
}

ObjectFallLayer::~ObjectFallLayer() {
	printTelemetry();
	gpu.freeImage(baseDrop);
}

void ObjectFallLayer::setDims(uint32_t w, uint32_t h) {
	dropW = w;
	dropH = h;
	forceRedraw();
}

void ObjectFallLayer::setSpeed(uint32_t speed) {
	if (!speed)
		dropSpeed = height * 0.35; // hardcoded atm
	else
		dropSpeed = speed;
	forceRedraw();
}

void ObjectFallLayer::setCustomSpeed(uint32_t speed) {
	dropSpeed = speed / 4 * speedAmplifier;           // 1
	dropW     = (speed + 400) / 300 * widthAmplifier; // 1
	dropH     = speed / 3.2 * heightAmplifier;        // 0.875
	forceRedraw();
}

void ObjectFallLayer::setAmplifiers(float s, float w, float h, float r, float m) {
	speedAmplifier  = s;
	widthAmplifier  = w;
	heightAmplifier = h;
	randomAmplifier = r;
	windAmplifier   = m;

	assert(heightAmplifier > 0);
}

void ObjectFallLayer::setAmount(uint32_t dropNum) {
	// Applies to each size
	if (randomAmplifier != 0)
		dropNum *= 3;

	dropAmount = dropNum;
	dropSpawnOrder.clear();
	forceRedraw();

	// Create the drop spawn order list.
	// This specifies the order of the positions along the sky axis to make the drops fall from.
	// By having a shuffled list rather than just using a rand function to determine the position, we aim for greater "evenness" and avoid empty spots.
	for (uint32_t i = 0; i < dropNum; i++) dropSpawnOrder.emplace_back(i);
	std::random_device rng;
	std::default_random_engine urng(rng());
	std::shuffle(dropSpawnOrder.begin(), dropSpawnOrder.end(), urng);
}

void ObjectFallLayer::setWind(int32_t factor) {
	// We will create a rotated coordinate system so that the drops always fall downwards and (0,0) is at the topleft of the bounding box containing the screen
	// e.g.
	//        ^^ windFactor θ = 135°         (0,0)___._  .=top     here the diamond is the original screen and the rectangle is the new bounding box.
	//    ___//__                                 | /\φ|           the drops fall from top to bottom ("fall axis")
	//   |  //   |               -->         left ./||\. right     exactly parallel to left and right
	//   .__/θ)__|                                |\vv/|           and between the bounds left and right ("sky axis")
	//top^  |/                                    |_\/_|
	//   =(0,1080)                                  bottom         φ = 135° % 90 = 45°

	MathVector<float> corners[]{MathVector<float>(0, 0),
	                            MathVector<float>(0, height),
	                            MathVector<float>(width, height),
	                            MathVector<float>(width, 0)};

	for (int i = 0; i < 3; i++) {
		auto &transform = transforms[i];
		auto realFactor = factor - randomAmplifier * (i - 1) * factor * windAmplifier;

		float radians    = realFactor * transFactor * M_PI / 180.0;
		transform.sin    = std::sin(radians);
		transform.cos    = std::cos(radians);
		transform.factor = realFactor;

		// Find which corners will be the top, left, bottom, and right corners after the rotation into the ij coordinate system
		// This depends on which quadrant (0~90, 90~180, etc) the angle is in.

		int a        = std::remainder(realFactor * transFactor, 360) + 360;
		int quadrant = a / 90 % 4;

		transform.top         = corners[quadrant];
		transform.left        = corners[(quadrant + 1) % 4];
		transform.bottom      = corners[(quadrant + 2) % 4];
		transform.right       = corners[(quadrant + 3) % 4];
		transform.originalTop = transform.top; // save top's original XY position, we will need it later for the reverse transformation

		// Translate these points into the ij coordinate system
		for (auto point : {&transform.top, &transform.left, &transform.bottom, &transform.right}) {
			MathVector<float> &ref = *point;
			ref                    = (ref - transform.originalTop).rotate(transform.sin, transform.cos);
		}
		for (auto point : {&transform.top, &transform.bottom, &transform.right, &transform.left}) { // do left last as it's used in the calculation
			MathVector<float> &ref = *point;
			ref                    = ref.translate(-transform.left.x, 0);
		}
	}
	// All drops from this point on will now be created with this wind.
	// But changing the wind later won't affect drops that were already created.
	forceRedraw();
}

void ObjectFallLayer::setBaseDrop(RenderImage *newBaseDrop) {
	drops.clear();
	gpu.freeImage(baseDrop);
	baseDrop = newBaseDrop;
	dropW    = newBaseDrop->w;
	dropH    = newBaseDrop->h;
	forceRedraw();
}

void ObjectFallLayer::setBaseDrop(SDL_Color &colour, uint32_t w, uint32_t h) {
	drops.clear();
	//TODO: add some gradients?
	if (baseDrop->w != w || baseDrop->h != h) {
		gpu.freeImage(baseDrop);
		baseDrop = gpu.createImage(baseDropWidth, baseDropHeight, 4);
		GPU_GetTarget(baseDrop);
	}

	gpu.clear(baseDrop->target, colour.r, colour.g, colour.b, colour.a);
	dropW = baseDrop->w;
	dropH = baseDrop->h;
	forceRedraw();
}

void ObjectFallLayer::setPause(bool state) {
	if (paused[CurrentScene] == state)
		return;
	paused[FormerScene]  = paused[CurrentScene];
	paused[CurrentScene] = state;
	old_drops.set(drops);
	if (sprite && sprite->exists)
		ons.backupState(sprite);
	forceRedraw();
}

void ObjectFallLayer::setBlend(BlendModeId mode) {
	blendMode = mode;
	forceRedraw();
}

void ObjectFallLayer::coverScreen() {
	uint32_t num = height / dropH * 3;
	for (uint32_t i = 0; i < num; i++)
		updateDrops(true, 1.0, true);
	forceRedraw();
}

bool ObjectFallLayer::update(bool old) {
	if (collectTelemetry)
		++telemetry.updateCalls;
	const size_t scene = sceneIndex(old);
	if (paused[scene]) {
		if (!forceNextUpdate[scene]) {
			if (collectTelemetry)
				++telemetry.pausedSkips;
			return false;
		}
		forceNextUpdate[scene] = false;
		if (collectTelemetry)
			++telemetry.pausedRedraws;
		return true;
	}

	const double scale = movementScale();
	const bool forcedRedraw = forceNextUpdate[scene];
	if (collectTelemetry) {
		if (forcedRedraw)
			++telemetry.immediateRedraws;
		else
			++telemetry.authoredStepRedraws;
	}
	forceNextUpdate[scene] = false;
	return updateDrops(old, scale);
}

double ObjectFallLayer::movementScale() const {
	if (!usesScriptFramePacing()) {
		return 1.0;
	}

	const double scale = ons.currentScriptFrameDeltaScale();
	return scale > 0.0 ? scale : 1.0;
}

bool ObjectFallLayer::usesScriptFramePacing() const {
	return sprite && sprite->duration_list &&
	       sprite->current_cell >= 0 && sprite->current_cell < sprite->num_of_cells &&
	       sprite->duration_list[sprite->current_cell] < 0;
}

size_t ObjectFallLayer::sceneIndex(bool old) const {
	return (old && old_drops.has()) ? FormerScene : CurrentScene;
}

void ObjectFallLayer::forceRedraw() {
	if (collectTelemetry)
		++telemetry.forcedRedrawRequests;
	forceNextUpdate[CurrentScene] = true;
	forceNextUpdate[FormerScene]  = true;
}

bool ObjectFallLayer::updateDrops(bool old, double movementScale, bool ignorePause) {
	const size_t scene = sceneIndex(old);
	if (paused[scene] && !ignorePause)
		return false;

	auto &rdrops       = scene == FormerScene ? old_drops.get() : drops;
	if (collectTelemetry)
		++telemetry.updateDropsCalls;

	// Firstly, remove the drops that have dropped offscreen (past their jMax)
	for (auto it = rdrops.begin(); it != rdrops.end();) {
		float topJ = it->j - it->h / 2.0;
		if (topJ >= it->jMax) {
			std::swap(*it, rdrops.back());
			rdrops.pop_back();
		} else {
			++it;
		}
	}

	// Secondly, move the drops
	for (auto &drop : rdrops) {
		drop.j += (dropSpeed + dropSpeed * drop.r) * movementScale;
	}

	// Thirdly, add the necessary drops
	while (rdrops.size() < dropAmount) {
		Drop d;
		int r           = std::rand() % 100;
		auto &transform = transforms[r % 3];
		d.r             = ((r % 3) - 1) * randomAmplifier;
		d.w             = dropW + d.r * dropW;
		d.h             = dropH + d.r * dropH;

		// Get a spawn position and cycle the list
		if (dropSpawnOrder.size() != dropAmount) {
			setAmount(dropAmount);
		} // might happen on usage of default value
		double mySpawnOrder = static_cast<double>(dropSpawnOrder.front());
		dropSpawnOrder.pop_front();
		if (overlapForcePercentage && mySpawnOrder && static_cast<uint32_t>(r) < overlapForcePercentage) {
			// Make another one here soon! (Creates consecutive drops next to each other for "longer rain streaks")
			dropSpawnOrder.insert(dropSpawnOrder.begin() + (std::rand() % overlapForceProximity), mySpawnOrder);
		} else {
			// To the back (now zero is guaranteed to come before this order comes again, meaning jiggle will be changed and we will not occupy the same i-pos again)
			dropSpawnOrder.push_back(mySpawnOrder);
		}

		// Once per cycle of the order queue, change the random jiggle value
		if (mySpawnOrder == 0) {
			currentJiggle = (std::rand() % 10000) / 10000.0;
		}

		// Jiggle makes rain not fall in predictable vertical slots all the time, by slightly adjusting the i-position for each slot
		auto thisJiggle = (mySpawnOrder + 1) * currentJiggle;
		thisJiggle      = thisJiggle - static_cast<long>(thisJiggle); // fractional part only
		mySpawnOrder += thisJiggle;

		// Position the drop in its ij coordinate system
		// CHECKME: should i be added d.w/2.0?
		float renderPosI = ((mySpawnOrder / dropAmount) * transform.right.x) - (d.w / 2.0);
		float renderPosJ = -(d.h / 2.0);

		// To prevent all the drops appearing at the same time at the start, the more there are left to add, the higher they should be added
		// (plus a random factor to help remove any random clustering that might happen)
		auto remaining = (dropAmount - rdrops.size()) - 1;
		renderPosJ -= (((std::rand() % 5) + 1) * remaining * transform.bottom.y) / dropAmount;

		d.i           = renderPosI;
		d.j           = renderPosJ;
		d.top         = transform.top;
		d.originalTop = transform.originalTop;
		d.jMax        = transform.bottom.y;
		d.angle       = transform.factor * -transFactor;
		d.sin         = transform.sin;
		d.cos         = transform.cos;
		cacheDropGeometry(d);
		rdrops.push_back(d);
	}

	return true;
}

void ObjectFallLayer::cacheDropGeometry(Drop &drop) const {
	constexpr float pi     = 3.14159265358979323846f;
	const float halfW      = drop.w * 0.5f;
	const float halfH      = drop.h * 0.5f;
	const float radians    = drop.angle * pi / 180.0f;
	const float cosA       = std::cos(radians);
	const float sinA       = std::sin(radians);
	const float local[4][2]{{-halfW, -halfH}, {halfW, -halfH}, {-halfW, halfH}, {halfW, halfH}};

	for (int i = 0; i < 4; ++i) {
		const float localX = local[i][0];
		const float localY = local[i][1];
		drop.cornerX[i]    = localX * cosA - localY * sinA;
		drop.cornerY[i]    = localX * sinA + localY * cosA;
	}
}

void ObjectFallLayer::renderDrops(RenderTarget *target, RenderRect &clip, float x, float y, std::vector<Drop> &rdrops) {
	if (clip.w == 0 || clip.h == 0 || rdrops.empty())
		return;

	if (collectTelemetry) {
		++telemetry.refreshCalls;
		telemetry.dropsRendered += rdrops.size();
	}

	if (target == ons.screen_target) {
		if (collectTelemetry)
			++telemetry.screenFallbackRefreshes;
		for (auto &drop : rdrops) {
			auto v = (drop.pos() - drop.top).rotate(-drop.sin, drop.cos) + drop.originalTop;
			gpu.copyGPUImage(baseDrop, nullptr, &clip, target, v.x + x, v.y + y,
			                 drop.w / static_cast<float>(baseDrop->w), drop.h / static_cast<float>(baseDrop->h), drop.angle, true);
		}
		return;
	}
	if (collectTelemetry)
		++telemetry.triangleBatchRefreshes;

	if (!(target->use_clip_rect && target->clip_rect.x == clip.x && target->clip_rect.y == clip.y &&
	      target->clip_rect.w == clip.w && target->clip_rect.h == clip.h)) {
		GPU_SetClipRect(target, clip);
	}

	batchVertices.clear();
	batchIndices.clear();
	batchVertices.reserve(rdrops.size() * 4);
	batchIndices.reserve(rdrops.size() * 6);

	const float clipRight  = clip.x + clip.w;
	const float clipBottom = clip.y + clip.h;
	gpu.setBlendMode(baseDrop);
	const SDL_Color color = baseDrop->color;
	const float r         = color.r / 255.0f;
	const float g         = color.g / 255.0f;
	const float b         = color.b / 255.0f;
	const float a         = color.a / 255.0f;

	auto flushBatch = [&]() {
		if (batchIndices.empty())
			return;
		GPU_TelemetryScope telemetryScope("objectfall_triangle_batch");
		GPU_TriangleBatchRGBA(baseDrop, target,
		                      static_cast<unsigned short>(batchVertices.size()),
		                      batchVertices.data(),
		                      static_cast<unsigned int>(batchIndices.size()),
		                      batchIndices.data());
		batchVertices.clear();
		batchIndices.clear();
	};

	for (auto &drop : rdrops) {
		// Transform the drop's coordinate system back into xy coordinates
		auto v = (drop.pos() - drop.top).rotate(-drop.sin, drop.cos) + drop.originalTop;
		const float centerX = v.x + x;
		const float centerY = v.y + y;

		float minX = std::numeric_limits<float>::max();
		float minY = std::numeric_limits<float>::max();
		float maxX = std::numeric_limits<float>::lowest();
		float maxY = std::numeric_limits<float>::lowest();
		float xy[4][2]{};
		for (int i = 0; i < 4; ++i) {
			xy[i][0] = centerX + drop.cornerX[i];
			xy[i][1] = centerY + drop.cornerY[i];
			minX     = std::min(minX, xy[i][0]);
			minY     = std::min(minY, xy[i][1]);
			maxX     = std::max(maxX, xy[i][0]);
			maxY     = std::max(maxY, xy[i][1]);
		}
		if (maxX < clip.x || maxY < clip.y || minX > clipRight || minY > clipBottom)
			continue;

		if (batchVertices.size() + 4 > 60000)
			flushBatch();

		const uint16_t base = static_cast<uint16_t>(batchVertices.size());
		const float uv[4][2]{{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}};
		for (int i = 0; i < 4; ++i) {
			batchVertices.push_back({xy[i][0], xy[i][1], r, g, b, a, uv[i][0], uv[i][1]});
		}
		const uint16_t indices[6]{
		    base,
		    static_cast<uint16_t>(base + 1),
		    static_cast<uint16_t>(base + 2),
		    static_cast<uint16_t>(base + 2),
		    static_cast<uint16_t>(base + 1),
		    static_cast<uint16_t>(base + 3),
		};
		batchIndices.insert(batchIndices.end(), indices, indices + 6);
	}
	flushBatch();
}

void ObjectFallLayer::refresh(RenderTarget *target, RenderRect &clip, float x, float y, bool /*centre_coordinates*/, int rm, float /*scalex*/, float /*scaley*/) {
	bool scene   = (rm & REFRESH_BEFORESCENE_MODE && old_drops.has());
	auto &rdrops = scene ? old_drops.get() : drops;

	if (clip.w == 0 || clip.h == 0 || rdrops.empty())
		return;

	renderDrops(target, clip, x, y, rdrops);
}

bool ObjectFallLayer::telemetryEnabled() const {
	const char *value = onsSDLGetEnv("ONS_SDL3_GPU_TELEMETRY");
	return value && *value && std::strcmp(value, "0") != 0;
}

void ObjectFallLayer::printTelemetry() const {
	if (!collectTelemetry || telemetryPrinted || telemetry.updateCalls == 0)
		return;
	telemetryPrinted = true;

	const int layerNo = sprite ? sprite->layer_no : -1;
	sendToLog(LogLevel::Info,
	          "ObjectFall telemetry: layer=%d update_calls=%llu skipped_fractional_updates=%llu "
	          "paused_redraws=%llu paused_skips=%llu immediate_redraws=%llu authored_step_redraws=%llu "
	          "update_drops_calls=%llu refresh_calls=%llu triangle_batch_refreshes=%llu "
	          "screen_fallback_refreshes=%llu drops_rendered=%llu forced_redraw_requests=%llu\n",
	          layerNo,
	          static_cast<unsigned long long>(telemetry.updateCalls),
	          static_cast<unsigned long long>(telemetry.skippedFractionalUpdates),
	          static_cast<unsigned long long>(telemetry.pausedRedraws),
	          static_cast<unsigned long long>(telemetry.pausedSkips),
	          static_cast<unsigned long long>(telemetry.immediateRedraws),
	          static_cast<unsigned long long>(telemetry.authoredStepRedraws),
	          static_cast<unsigned long long>(telemetry.updateDropsCalls),
	          static_cast<unsigned long long>(telemetry.refreshCalls),
	          static_cast<unsigned long long>(telemetry.triangleBatchRefreshes),
	          static_cast<unsigned long long>(telemetry.screenFallbackRefreshes),
	          static_cast<unsigned long long>(telemetry.dropsRendered),
	          static_cast<unsigned long long>(telemetry.forcedRedrawRequests));
}

void ObjectFallLayer::commit() {
	//sendToLog(LogLevel::Info, "Time to commit ObjectFallLayer\n");
	if (paused[CurrentScene] != paused[FormerScene]) {
		old_drops.unset();
		paused[FormerScene] = paused[CurrentScene];
	}
}

std::unordered_map<std::string, DynamicPropertyInterface> ObjectFallLayer::properties() {
	return {
	    {"fallamount",
	     {[](void *layer) -> double {
		      auto fall = static_cast<ObjectFallLayer *>(layer);
		      return fall->dropAmount;
	      },
	      [](void *layer, double value) -> void {
		      auto fall = static_cast<ObjectFallLayer *>(layer);
		      fall->setAmount(value);
	      }}}};
}
