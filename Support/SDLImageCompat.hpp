/**
 *  SDLImageCompat.hpp
 *  ONScripter-RU
 *
 *  SDL_image include boundary for the SDL2-to-SDL3 migration.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "Support/SDLCompat.hpp"

#if defined(ONS_USE_SDL3)
	#include <SDL3_image/SDL_image.h>

	#ifndef IMG_Load_RW
		#define IMG_Load_RW(src, close_src) IMG_Load_IO((src), (close_src))
	#endif
	#ifndef IMG_LoadJPG_RW
		#define IMG_LoadJPG_RW(src) IMG_LoadJPG_IO(src)
	#endif
	#ifndef IMG_GetError
		#define IMG_GetError SDL_GetError
	#endif
#else
	#include <SDL2/SDL_image.h>
#endif
