/**
 *  SDLCompat.hpp
 *  ONScripter-RU
 *
 *  SDL runtime include boundary for the SDL2-to-SDL3 migration.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#if defined(ONS_USE_SDL3)
	#ifndef SDL_ENABLE_OLD_NAMES
		#define SDL_ENABLE_OLD_NAMES 1
	#endif
	#include <SDL3/SDL.h>

	using SDL_semaphore = SDL_Semaphore;

	#ifndef SDL_SWSURFACE
		#define SDL_SWSURFACE 0
	#endif
	#ifndef SDL_MUTEX_TIMEDOUT
		#define SDL_MUTEX_TIMEDOUT 1
	#endif
	#ifndef SDL_INIT_TIMER
		#define SDL_INIT_TIMER 0
	#endif
	#ifndef AUDIO_U16
		#define AUDIO_U16 static_cast<SDL_AudioFormat>(0x0010)
	#endif
	#ifndef SDL_mutexP
		#define SDL_mutexP(mutex) (SDL_LockMutex(mutex), 0)
	#endif
	#ifndef SDL_mutexV
		#define SDL_mutexV(mutex) (SDL_UnlockMutex(mutex), 0)
	#endif

	static constexpr int ONS_MULTIGESTURE_EVENT = SDL_EVENT_PINCH_UPDATE;
#else
	#include <SDL2/SDL.h>

static constexpr int ONS_MULTIGESTURE_EVENT = SDL_MULTIGESTURE;
#endif

inline SDL_Scancode &onsKeyboardScancode(SDL_KeyboardEvent &event) {
#if defined(ONS_USE_SDL3)
	return event.scancode;
#else
	return event.keysym.scancode;
#endif
}

inline SDL_Scancode onsKeyboardScancode(const SDL_KeyboardEvent &event) {
#if defined(ONS_USE_SDL3)
	return event.scancode;
#else
	return event.keysym.scancode;
#endif
}

inline SDL_FingerID &onsTouchFingerId(SDL_TouchFingerEvent &event) {
#if defined(ONS_USE_SDL3)
	return event.fingerID;
#else
	return event.fingerId;
#endif
}

inline SDL_FingerID onsTouchFingerId(const SDL_TouchFingerEvent &event) {
#if defined(ONS_USE_SDL3)
	return event.fingerID;
#else
	return event.fingerId;
#endif
}

inline Uint32 onsGetMouseState(int *x, int *y) {
#if defined(ONS_USE_SDL3)
	float fx = 0;
	float fy = 0;
	Uint32 state = SDL_GetMouseState(&fx, &fy);
	if (x)
		*x = static_cast<int>(fx);
	if (y)
		*y = static_cast<int>(fy);
	return state;
#else
	return SDL_GetMouseState(x, y);
#endif
}

inline int onsGetNumVideoDisplays() {
#if defined(ONS_USE_SDL3)
	int count = 0;
	SDL_DisplayID *displays = SDL_GetDisplays(&count);
	SDL_free(displays);
	return count;
#else
	return SDL_GetNumVideoDisplays();
#endif
}

inline Uint32 onsDisplayIdByIndex(int displayIndex) {
#if defined(ONS_USE_SDL3)
	int count = 0;
	SDL_DisplayID *displays = SDL_GetDisplays(&count);
	Uint32 displayId = (displays && displayIndex >= 0 && displayIndex < count) ? displays[displayIndex] : 0;
	SDL_free(displays);
	return displayId;
#else
	return displayIndex;
#endif
}

inline bool onsGetDesktopDisplayMode(int displayIndex, SDL_DisplayMode *mode) {
#if defined(ONS_USE_SDL3)
	Uint32 displayId = onsDisplayIdByIndex(displayIndex);
	const SDL_DisplayMode *desktopMode = displayId ? SDL_GetDesktopDisplayMode(displayId) : nullptr;
	if (!desktopMode || !mode)
		return false;
	*mode = *desktopMode;
	return true;
#else
	return SDL_GetDesktopDisplayMode(displayIndex, mode) == 0;
#endif
}

inline bool onsGetDisplayBounds(int displayIndex, SDL_Rect *rect) {
#if defined(ONS_USE_SDL3)
	Uint32 displayId = onsDisplayIdByIndex(displayIndex);
	return displayId && SDL_GetDisplayBounds(displayId, rect);
#else
	return SDL_GetDisplayBounds(displayIndex, rect) == 0;
#endif
}

inline int onsNumJoysticks() {
#if defined(ONS_USE_SDL3)
	int count = 0;
	SDL_JoystickID *joysticks = SDL_GetJoysticks(&count);
	SDL_free(joysticks);
	return count;
#else
	return SDL_NumJoysticks();
#endif
}

inline SDL_JoystickID onsJoystickIdByIndex(int joystickIndex) {
#if defined(ONS_USE_SDL3)
	int count = 0;
	SDL_JoystickID *joysticks = SDL_GetJoysticks(&count);
	SDL_JoystickID joystickId = (joysticks && joystickIndex >= 0 && joystickIndex < count) ? joysticks[joystickIndex] : 0;
	SDL_free(joysticks);
	return joystickId;
#else
	return joystickIndex;
#endif
}

inline SDL_Joystick *onsJoystickOpenByIndex(int joystickIndex) {
#if defined(ONS_USE_SDL3)
	SDL_JoystickID joystickId = onsJoystickIdByIndex(joystickIndex);
	return joystickId ? SDL_OpenJoystick(joystickId) : nullptr;
#else
	return SDL_JoystickOpen(joystickIndex);
#endif
}

inline const char *onsJoystickNameForIndex(int joystickIndex) {
#if defined(ONS_USE_SDL3)
	SDL_JoystickID joystickId = onsJoystickIdByIndex(joystickIndex);
	return joystickId ? SDL_GetJoystickNameForID(joystickId) : nullptr;
#else
	return SDL_JoystickNameForIndex(joystickIndex);
#endif
}

inline bool onsSetWindowFullscreen(SDL_Window *window, bool fullscreen) {
#if defined(ONS_USE_SDL3)
	return SDL_SetWindowFullscreen(window, fullscreen);
#else
	return SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) == 0;
#endif
}

inline bool onsStartTextInput(SDL_Window *window = nullptr) {
#if defined(ONS_USE_SDL3)
	return window ? SDL_StartTextInput(window) : false;
#else
	(void)window;
	SDL_StartTextInput();
	return true;
#endif
}

inline bool onsStopTextInput(SDL_Window *window = nullptr) {
#if defined(ONS_USE_SDL3)
	return window ? SDL_StopTextInput(window) : false;
#else
	(void)window;
	SDL_StopTextInput();
	return true;
#endif
}

inline bool onsShowCursor(bool show) {
#if defined(ONS_USE_SDL3)
	return show ? SDL_ShowCursor() : SDL_HideCursor();
#else
	return SDL_ShowCursor(show ? SDL_ENABLE : SDL_DISABLE) >= 0;
#endif
}

inline int &onsMessageBoxButtonId(SDL_MessageBoxButtonData &button) {
#if defined(ONS_USE_SDL3)
	return button.buttonID;
#else
	return button.buttonid;
#endif
}

inline int onsSDLSetEnv(const char *name, const char *value, bool overwrite) {
#if defined(ONS_USE_SDL3)
	return SDL_setenv_unsafe(name, value, overwrite ? 1 : 0);
#else
	return SDL_setenv(name, value, overwrite ? 1 : 0);
#endif
}

inline const char *onsSDLGetEnv(const char *name) {
#if defined(ONS_USE_SDL3)
	const char *value = SDL_getenv(name);
	return value ? value : SDL_getenv_unsafe(name);
#else
	return SDL_getenv(name);
#endif
}

inline size_t onsRWread(SDL_RWops *context, void *ptr, size_t size, size_t maxnum) {
#if defined(ONS_USE_SDL3)
	if (size == 0)
		return 0;
	return SDL_ReadIO(context, ptr, size * maxnum) / size;
#else
	return SDL_RWread(context, ptr, size, maxnum);
#endif
}

inline size_t onsRWwrite(SDL_RWops *context, const void *ptr, size_t size, size_t num) {
#if defined(ONS_USE_SDL3)
	if (size == 0)
		return 0;
	return SDL_WriteIO(context, ptr, size * num) / size;
#else
	return SDL_RWwrite(context, ptr, size, num);
#endif
}

inline Uint32 onsWindowEventType(const SDL_WindowEvent &event) {
#if defined(ONS_USE_SDL3)
	return event.type;
#else
	return event.event;
#endif
}

inline int onsSurfaceBitsPerPixel(const SDL_Surface *surface) {
#if defined(ONS_USE_SDL3)
	const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);
	return format ? format->bits_per_pixel : 0;
#else
	return surface->format->BitsPerPixel;
#endif
}

inline int onsSurfaceBytesPerPixel(const SDL_Surface *surface) {
#if defined(ONS_USE_SDL3)
	const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);
	return format ? format->bytes_per_pixel : 0;
#else
	return surface->format->BytesPerPixel;
#endif
}

inline Uint32 onsSurfaceRmask(const SDL_Surface *surface) {
#if defined(ONS_USE_SDL3)
	const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);
	return format ? format->Rmask : 0;
#else
	return surface->format->Rmask;
#endif
}

inline Uint32 onsSurfaceGmask(const SDL_Surface *surface) {
#if defined(ONS_USE_SDL3)
	const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);
	return format ? format->Gmask : 0;
#else
	return surface->format->Gmask;
#endif
}

inline Uint32 onsSurfaceBmask(const SDL_Surface *surface) {
#if defined(ONS_USE_SDL3)
	const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);
	return format ? format->Bmask : 0;
#else
	return surface->format->Bmask;
#endif
}

inline Uint32 onsSurfaceAmask(const SDL_Surface *surface) {
#if defined(ONS_USE_SDL3)
	const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);
	return format ? format->Amask : 0;
#else
	return surface->format->Amask;
#endif
}

inline Uint8 onsSurfaceRshift(const SDL_Surface *surface) {
#if defined(ONS_USE_SDL3)
	const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);
	return format ? format->Rshift : 0;
#else
	return surface->format->Rshift;
#endif
}

inline Uint8 onsSurfaceGshift(const SDL_Surface *surface) {
#if defined(ONS_USE_SDL3)
	const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);
	return format ? format->Gshift : 0;
#else
	return surface->format->Gshift;
#endif
}

inline Uint8 onsSurfaceBshift(const SDL_Surface *surface) {
#if defined(ONS_USE_SDL3)
	const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);
	return format ? format->Bshift : 0;
#else
	return surface->format->Bshift;
#endif
}

inline Uint8 onsSurfaceAshift(const SDL_Surface *surface) {
#if defined(ONS_USE_SDL3)
	const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);
	return format ? format->Ashift : 0;
#else
	return surface->format->Ashift;
#endif
}

inline Uint8 onsSurfaceRloss(const SDL_Surface *surface) {
#if defined(ONS_USE_SDL3)
	const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);
	return format ? static_cast<Uint8>(8 - format->Rbits) : 0;
#else
	return surface->format->Rloss;
#endif
}

inline Uint8 onsSurfaceGloss(const SDL_Surface *surface) {
#if defined(ONS_USE_SDL3)
	const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);
	return format ? static_cast<Uint8>(8 - format->Gbits) : 0;
#else
	return surface->format->Gloss;
#endif
}

inline Uint8 onsSurfaceBloss(const SDL_Surface *surface) {
#if defined(ONS_USE_SDL3)
	const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);
	return format ? static_cast<Uint8>(8 - format->Bbits) : 0;
#else
	return surface->format->Bloss;
#endif
}

inline SDL_Palette *onsSurfacePalette(SDL_Surface *surface) {
#if defined(ONS_USE_SDL3)
	return SDL_GetSurfacePalette(surface);
#else
	return surface->format->palette;
#endif
}

inline bool onsSetPaletteColors(SDL_Palette *palette, const SDL_Color *colors, int firstColor, int numColors) {
#if defined(ONS_USE_SDL3)
	return SDL_SetPaletteColors(palette, colors, firstColor, numColors);
#else
	return SDL_SetPaletteColors(palette, colors, firstColor, numColors) == 0;
#endif
}

inline bool onsGetColorKey(SDL_Surface *surface, Uint32 *key) {
#if defined(ONS_USE_SDL3)
	return SDL_GetSurfaceColorKey(surface, key);
#else
	return SDL_GetColorKey(surface, key) == 0;
#endif
}

inline bool onsSetColorKey(SDL_Surface *surface, bool enabled, Uint32 key) {
#if defined(ONS_USE_SDL3)
	return SDL_SetSurfaceColorKey(surface, enabled, key);
#else
	return SDL_SetColorKey(surface, enabled ? SDL_TRUE : SDL_FALSE, key) == 0;
#endif
}

inline bool onsSDLInit(Uint32 flags) {
#if defined(ONS_USE_SDL3)
	return SDL_Init(static_cast<SDL_InitFlags>(flags));
#else
	return SDL_Init(flags) == 0;
#endif
}

inline bool onsSDLInitSubSystem(Uint32 flags) {
#if defined(ONS_USE_SDL3)
	return SDL_InitSubSystem(static_cast<SDL_InitFlags>(flags));
#else
	return SDL_InitSubSystem(flags) == 0;
#endif
}

inline int onsWaitSemaphoreTimeoutResult(SDL_sem *sem, int timeoutMS) {
#if defined(ONS_USE_SDL3)
	return SDL_WaitSemaphoreTimeout(sem, timeoutMS) ? 0 : SDL_MUTEX_TIMEDOUT;
#else
	return SDL_SemWaitTimeout(sem, static_cast<Uint32>(timeoutMS));
#endif
}

inline int onsWaitSemaphore(SDL_sem *sem) {
#if defined(ONS_USE_SDL3)
	SDL_WaitSemaphore(sem);
	return 0;
#else
	return SDL_SemWait(sem);
#endif
}

inline bool onsTryWaitSemaphore(SDL_sem *sem) {
#if defined(ONS_USE_SDL3)
	return SDL_TryWaitSemaphore(sem);
#else
	return SDL_SemTryWait(sem) == 0;
#endif
}

inline bool onsWaitSemaphoreTimeout(SDL_sem *sem, int timeoutMS) {
	return onsWaitSemaphoreTimeoutResult(sem, timeoutMS) == 0;
}

inline bool onsShowMessageBox(const SDL_MessageBoxData *data, int *buttonId) {
#if defined(ONS_USE_SDL3)
	return SDL_ShowMessageBox(data, buttonId);
#else
	return SDL_ShowMessageBox(data, buttonId) == 0;
#endif
}

inline bool onsShowSimpleMessageBox(Uint32 flags, const char *title, const char *message, SDL_Window *window) {
#if defined(ONS_USE_SDL3)
	return SDL_ShowSimpleMessageBox(static_cast<SDL_MessageBoxFlags>(flags), title, message, window);
#else
	return SDL_ShowSimpleMessageBox(flags, title, message, window) == 0;
#endif
}

inline Uint32 onsMasksToPixelFormatEnum(int bitsPerPixel, Uint32 rmask, Uint32 gmask, Uint32 bmask, Uint32 amask) {
#if defined(ONS_USE_SDL3)
	return static_cast<Uint32>(SDL_GetPixelFormatForMasks(bitsPerPixel, rmask, gmask, bmask, amask));
#else
	return SDL_MasksToPixelFormatEnum(bitsPerPixel, rmask, gmask, bmask, amask);
#endif
}

inline Uint32 onsSurfacePixelFormatEnum(const SDL_Surface *surface) {
#if defined(ONS_USE_SDL3)
	return static_cast<Uint32>(surface->format);
#else
	return surface->format->format;
#endif
}

inline SDL_Surface *onsCreateRGBSurface(Uint32 flags, int width, int height, int depth, Uint32 rmask, Uint32 gmask, Uint32 bmask, Uint32 amask) {
#if defined(ONS_USE_SDL3)
	(void)flags;
	SDL_PixelFormat format = static_cast<SDL_PixelFormat>(onsMasksToPixelFormatEnum(depth, rmask, gmask, bmask, amask));
	SDL_Surface *surface   = SDL_CreateSurface(width, height, format);
	if (surface && SDL_ISPIXELFORMAT_INDEXED(format) && !SDL_GetSurfacePalette(surface))
		SDL_CreateSurfacePalette(surface);
	return surface;
#else
	return SDL_CreateRGBSurface(flags, width, height, depth, rmask, gmask, bmask, amask);
#endif
}

inline SDL_Surface *onsConvertSurfaceFormat(SDL_Surface *surface, Uint32 format, Uint32 flags) {
#if defined(ONS_USE_SDL3)
	(void)flags;
	return SDL_ConvertSurface(surface, static_cast<SDL_PixelFormat>(format));
#else
	return SDL_ConvertSurfaceFormat(surface, format, flags);
#endif
}

inline SDL_Surface *onsConvertSurfaceLike(SDL_Surface *surface, const SDL_Surface *formatSource, Uint32 flags) {
#if defined(ONS_USE_SDL3)
	(void)flags;
	return SDL_ConvertSurface(surface, formatSource->format);
#else
	return SDL_ConvertSurface(surface, formatSource->format, flags);
#endif
}

inline Uint32 onsMapRGB(SDL_Surface *surface, Uint8 r, Uint8 g, Uint8 b) {
#if defined(ONS_USE_SDL3)
	const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);
	return SDL_MapRGB(format, SDL_GetSurfacePalette(surface), r, g, b);
#else
	return SDL_MapRGB(surface->format, r, g, b);
#endif
}

inline Uint32 onsMapRGBA(SDL_Surface *surface, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
#if defined(ONS_USE_SDL3)
	const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(surface->format);
	return SDL_MapRGBA(format, SDL_GetSurfacePalette(surface), r, g, b, a);
#else
	return SDL_MapRGBA(surface->format, r, g, b, a);
#endif
}
