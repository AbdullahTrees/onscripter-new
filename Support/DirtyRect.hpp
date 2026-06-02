/**
 *  DirtyRect.hpp
 *  ONScripter-RU
 *
 *  Invalid region on image target which should be updated.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"

#include "Support/SDLCompat.hpp"
#include "Engine/Graphics/RendererBackend.hpp"

struct DirtyRect {
	void setDimension(const SDL_Point &canvas, const RenderRect &camera_center);
	void add(RenderRect src);
	void clear();
	void fill(int w, int h);
	bool isEmpty();

	RenderRect calcBoundingBox(RenderRect src1, RenderRect &src2);

	SDL_Point canvas_dim{};
	RenderRect camera_center_pos{};
	RenderRect bounding_box{};
	RenderRect bounding_box_script{};
};
