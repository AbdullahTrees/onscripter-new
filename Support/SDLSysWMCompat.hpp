/**
 *  SDLSysWMCompat.hpp
 *  ONScripter-RU
 *
 *  SDL native-window include boundary for the SDL2-to-SDL3 migration.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "Support/SDLCompat.hpp"

#if !defined(ONS_USE_SDL3)
	#include <SDL2/SDL_syswm.h>
#endif
