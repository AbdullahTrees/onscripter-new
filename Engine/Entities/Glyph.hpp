/**
 *  Glyph.hpp
 *  ONScripter-RU
 *
 *  Glyph entity support split in params (pointers) and values (representations).
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"

#include "Support/SDLCompat.hpp"
#include "Engine/Graphics/RendererBackend.hpp"

#include <cstddef>
#include <cstdint>

class GlyphAtlasController;

//Key params of our Font cache
struct GlyphParams {
	uint32_t unicode;
	uint32_t font_number;
	int preset_id;
	int font_size;
	int border_width;
	SDL_Color glyph_color;
	SDL_Color border_color;

	bool is_bold : 1;
	bool is_italic : 1;
	bool is_underline : 1;
	bool is_border : 1;
	bool is_colored : 1;
	bool is_gradient : 1;
};

//Value params of our Font cache
class GlyphValues {
public:
	//glyph and glyph_gpu are same for the shadow
	RenderImage *glyph_gpu{nullptr};
	RenderImage *border_gpu{nullptr};

	// I don't like having these surfaces here either
	SDL_Surface *bitmap{nullptr};
	SDL_Surface *border_bitmap{nullptr};

	SDL_Point border_bitmap_offset{0, 0};
	cmp::optional<RenderRect> glyph_pos;
	cmp::optional<RenderRect> border_pos;

	//I don't like having this block here, but a unified Glyph class is worth it
	float minx{0}, maxx{0}, miny{0}, maxy{0}, advance{0}, faceAscender{0}, faceDescender{0};
	unsigned int ftCharIndexCache{0};

	bool buildGPUImage(bool border = false, GlyphAtlasController *atlas = nullptr);
	bool buildGPUImages(GlyphAtlasController *atlas = nullptr);
	GlyphValues() = default;
	GlyphValues(const GlyphValues &orig);
	GlyphValues &operator=(const GlyphValues &) = delete;
	~GlyphValues();
};

struct GlyphParamsHash {
	size_t operator()(const GlyphParams &gp) const {
		auto combine = [](size_t &seed, size_t value) {
			seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		};
		auto has_color = [](const GlyphParams &params) {
			return params.is_colored &&
			       !(params.glyph_color.r == 0 && params.glyph_color.g == 0 && params.glyph_color.b == 0 &&
			         params.border_color.r == 0 && params.border_color.g == 0 && params.border_color.b == 0);
		};

		size_t seed = 0;
		combine(seed, gp.unicode);
		combine(seed, gp.font_number);
		combine(seed, static_cast<size_t>(gp.preset_id));
		combine(seed, static_cast<size_t>(gp.font_size));
		combine(seed, static_cast<size_t>(gp.border_width));
		combine(seed, gp.is_bold);
		combine(seed, gp.is_italic);
		combine(seed, gp.is_underline);
		combine(seed, gp.is_border);

		const bool colored = has_color(gp);
		combine(seed, colored);
		if (colored) {
			combine(seed, gp.is_gradient);
			combine(seed, gp.glyph_color.r);
			combine(seed, gp.glyph_color.g);
			combine(seed, gp.glyph_color.b);
			combine(seed, gp.border_color.r);
			combine(seed, gp.border_color.g);
			combine(seed, gp.border_color.b);
		}

		return seed;
	}
};

struct GlyphParamsEqual {
	bool operator()(const GlyphParams &left, const GlyphParams &right) const {
		bool base_params_equal = (left.unicode == right.unicode &&
		                          left.font_number == right.font_number &&
		                          left.preset_id == right.preset_id &&
		                          left.font_size == right.font_size &&
		                          left.is_bold == right.is_bold &&
		                          left.is_italic == right.is_italic &&
		                          left.is_underline == right.is_underline &&
		                          left.is_border == right.is_border &&
		                          left.border_width == right.border_width);

		if (!base_params_equal)
			return false;

		bool left_no_color  = !left.is_colored || (left.glyph_color.r == 0 && left.glyph_color.g == 0 && left.glyph_color.b == 0 &&
                                                  left.border_color.r == 0 && left.border_color.g == 0 && left.border_color.b == 0);
		bool right_no_color = !right.is_colored || (right.glyph_color.r == 0 && right.glyph_color.g == 0 && right.glyph_color.b == 0 &&
		                                            right.border_color.r == 0 && right.border_color.g == 0 && right.border_color.b == 0);

		if (left_no_color != right_no_color)
			return false;

		bool color_equal = (left.is_colored && right.is_colored &&
		                    left.is_gradient == right.is_gradient &&
		                    left.glyph_color.r == right.glyph_color.r &&
		                    left.glyph_color.g == right.glyph_color.g &&
		                    left.glyph_color.b == right.glyph_color.b &&
		                    left.border_color.r == right.border_color.r &&
		                    left.border_color.g == right.border_color.g &&
		                    left.border_color.b == right.border_color.b);

		return (left_no_color && right_no_color) || color_equal;
	}
};
