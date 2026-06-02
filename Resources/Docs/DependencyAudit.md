# Dependency Audit and Modernization Plan

Date: 2026-05-31
Updated: 2026-06-02

This document is the current dependency and renderer modernization status for
`onscripter-new`. It is intentionally kept as a clean status record, not a
complete investigation log.

## Current Status

- The supported platform floors have been raised and encoded in the build
  scripts, packaging scripts, and docs.
- The low-risk compression/media leaf libraries, text stack, FFmpeg, SDL2 stack,
  and temporary SDL2_gpu support dependencies have been updated.
- smpeg2 has been removed from package metadata, engine includes, configure
  metadata, and Xcode references.
- SDL3, SDL3_image, and SDL3_mixer are available behind `--sdl3-renderer`.
- SDL3 has not fully replaced SDL2 yet. The default renderer remains the
  temporary SDL2_gpu path until final SDL3 renderer cutover and broader
  regression testing are complete.
- The SDL3 Windows public-release executable has booted Umineko Project release
  data to the main menu with SDL3_GPU rendering and SDL3_mixer audio active.

## Support Floor

| Platform | Support floor | Notes |
| --- | --- | --- |
| Windows | Windows 10 | Prefer x86_64 UCRT64 builds. `_WIN32_WINNT` and `WINVER` are set to `0x0A00`. |
| macOS | macOS 14 | x86_64 remains supported; arm64 is the target for Apple Silicon work. Legacy i386/custom libc++ support is removed from the active floor. |
| iOS | iOS 17 | arm64 devices and current simulator targets. armv7/armv7s are unsupported. |
| Android | Android 14/API 34 | arm64-v8a is the default target; x86_64 is available for emulator and development builds. |
| Debian | Debian 11 | Conservative Linux ABI/build floor for generic Linux binaries. |
| Ubuntu | Ubuntu 22.04 LTS | LTS baseline for generic Linux builds. |

## Dependency State

| Dependency | Current repo pin/status | Notes |
| --- | --- | --- |
| bzip2 | 1.0.8 | Updated leaf dependency. |
| zlib | 1.3.2 | Updated leaf dependency. |
| libpng | 1.6.58 | Updated leaf dependency. |
| libogg | 1.3.6 | Updated leaf dependency. |
| libvorbis | 1.3.7 | Updated leaf dependency. |
| FreeType | 2.14.3 | Updated as part of the text stack. |
| HarfBuzz | 14.2.0 | Updated; built with Meson/Ninja. |
| FriBidi | 1.0.16 | Updated as part of the text stack. |
| libass | 0.17.4 | Updated as part of the text/subtitle stack. |
| FFmpeg | 7.1.4 | Engine ported to current send/receive decode APIs and `AVChannelLayout`. Windows D3D11VA/DXVA2/Media Foundation/CUDA LLVM probing is disabled in the package. |
| SDL2 | 2.32.10 | Updated default runtime stack. |
| SDL2_image | 2.8.12 | Updated default image stack; uses audited libpng/jpeg dependencies. |
| SDL2_mixer | 2.8.2 | Updated default mixer stack; MP3 uses minimp3. |
| SDL2_gpu | forked 0.11.0 | Still the default temporary renderer backend. Keep only until SDL3 cutover. |
| libepoxy | 1.5.10 | Temporary SDL2_gpu dependency; remove with SDL2_gpu. |
| SDL3 | 3.4.10 | Staged behind `--sdl3-renderer`; built static with SDL_GPU and SDL renderer support enabled. |
| SDL3_image | 3.4.4 | Staged behind `--sdl3-renderer`; PNG/JPEG enabled through local dependencies. |
| SDL3_mixer | 3.2.2 | Staged behind `--sdl3-renderer`; WAV, MP3/dr_mp3, and Ogg Vorbis/libvorbisfile enabled. |
| smpeg2 | removed | Retired from dependency graph and build metadata. |
| jpeg | IJG 9c | Still pending replacement evaluation, likely libjpeg-turbo. |
| Lua | 5.3.5 | Pending compatibility review before any 5.4/5.5 move. |
| libusb | 1.0.22 | Pending update. |
| libunwind | Android legacy package | Pending removal if modern NDK unwinder coverage is sufficient. |
| libc++/libc++abi | legacy 8.0.0 packages | Pending removal with the old macOS/iOS floor cleanup. |

## Completed Work

Platform/build modernization:

- `configure` defaults Android builds to arm64, rejects unsupported Android
  architectures, detects Windows UCRT64/x86_64, and applies the Windows 10 API
  floor.
- `Dependencies/build.sh` defaults Apple deployment to macOS 14/iOS 17, rejects
  unsupported Apple 32-bit targets, defaults Android to modern arm64 toolchains,
  and recognizes Android x86_64.
- `Scripts/ndktoolchain.sh` targets NDK r29/API 34 and creates arm64/x86_64
  wrapper toolchains.
- Android packaging uses min SDK 34, target SDK 36, D8, apksigner/zipalign when
  available, and arm64-v8a/x86_64 packaging.
- Xcode project deployment targets were raised to macOS 14 and iOS 17.
- Windows helper packaging now points at MSYS2 UCRT64 and treats old
  pre-Windows-10 ANGLE folders as legacy test assets.

Dependency modernization:

- Low-risk bzip2, zlib, libpng, libogg, and libvorbis updates are complete.
- FreeType, HarfBuzz, FriBidi, and libass were updated together, with obsolete
  local patches removed or replaced by configure/cache options.
- FFmpeg was updated to 7.1.4 and the engine decode path was ported away from
  removed/deprecated FFmpeg 3.x APIs.
- SDL2, SDL2_image, and SDL2_mixer were updated and smpeg2 was removed.
- SDL2_gpu remains buildable with a modern CMake policy compatibility flag and
  libepoxy 1.5.10 while the SDL3 renderer path matures.

SDL3 source path:

- `--sdl3-renderer` selects SDL3, SDL3_image, and SDL3_mixer instead of the
  SDL2/SDL2_gpu stack.
- SDL compatibility headers now isolate most SDL2/SDL3 API differences:
  initialization returns, surface formats, palette setup, color keys, image
  loading/saving, mixer APIs, syswm access, events, window state, keyboard,
  mouse, touch, joystick, cursor, and text input.
- `Support/SDLMixerCompat.cpp` maps the engine's current SDL2_mixer-style calls
  onto SDL3_mixer's mixer/track/audio model for the call surface used by the
  engine.
- `Engine/Graphics/SDL3GPUCompat.cpp` provides the SDL3_GPU transition backend:
  texture/render-target ownership, CPU backing storage, uploads, presentation,
  native fixed-pipeline blits, `GPU_TriangleBatch`, render-target readback, and
  PNG screenshot output.
- Built-in project shader effects execute through the SDL3 compatibility path
  when SDL3_GPU cannot consume the original GLSL directly.
- Optional SDL_shadercross support can run external SPIR-V/HLSL shaders through
  native SDL_GPU pipelines. With shaderc available, modern Vulkan-layout GLSL
  can be compiled to SPIR-V at runtime first.
- Simple legacy SDL2_gpu-style fragment GLSL is translated for native execution.
  Arbitrary older OpenGL GLSL still needs source porting or a more complete
  translator.

Recent SDL3 runtime fixes:

- Public release builds are required for packaged `.file` scripts; development
  builds expect plaintext script layouts.
- `ScriptParser::resetDefineFlags()` now initializes `effect_links` before using
  `front()`, fixing a constructor-time debug assertion/undefined access.
- SDL3 indexed surfaces now get palettes attached through `onsCreateRGBSurface`,
  and palette writes use the SDL2/SDL3-compatible `onsSetPaletteColors` helper.
  This fixed the main-menu crash traced to `Font::freetypeToSDLSurface()`.
- Windows FFmpeg package configuration now explicitly disables D3D11VA, DXVA2,
  Media Foundation, and CUDA LLVM probing to avoid current UCRT header failures.
- Current MSYS2/UCRT64 GCC 16 Windows release builds should use
  `--std=gnu++14` instead of strict `--std=c++14`.

## Validation Snapshot

Dependency and build verification completed during this modernization pass:

- Native UCRT64 configure/build checks for the default SDL2 stack.
- Clean package builds for the low-risk leaf dependencies, text stack, FFmpeg
  7.1.4, SDL2 stack, SDL3 staging stack, and the temporary SDL2_gpu/libepoxy
  backend.
- Static link smoke tests for FFmpeg, libass, SDL2_image/SDL2_mixer, and related
  package metadata.
- SDL3 header and syntax probes for compatibility headers, renderer code, event
  code, window/input code, audio code, image paths, and SDL_mixer adapter paths.
- SDL3 renderer runtime probes for native blits, triangle batches, render-target
  readback, PNG screenshot signature validation, presentation, built-in shader
  compatibility, SPIR-V/HLSL shadercross paths, shaderc-backed GLSL, and simple
  legacy fragment GLSL translation.
- SDL3_mixer runtime probes for audio open/query, channel allocation, channel
  playback/halt callbacks, looping state, raw-stream effects, WAV music loading,
  music playback, and music finished callbacks.

Full game runtime validation:

- Built a Windows SDL3 public release with:

  ```
  ./configure --sdl3-renderer --release-build --strip-binary --std=gnu++14
  make -j8
  ```

- Installed the result as `D:\Umineko Project\onscripter-ru.exe`.
- Booted Umineko Project release data with `en.file` through the SDL3 build.
- The game reached and remained at the main menu for a 120 second validation
  run.
- The process stayed responsive; no new Windows Application Error was produced.
- The runtime log showed SDL3_GPU initialization, shader compilation, SDL3_mixer
  audio startup, layer effect setup, and speech-level activity.

## Packaging Notes

- The current Windows SDL3 build links SDL3, SDL3_image, and SDL3_mixer
  statically. A normal SDL3 package does not need an SDL DLL folder beside the
  executable.
- If optional shaderc support is enabled through MSYS2's `shaderc.pc`, packaging
  must include `libshaderc_shared.dll` or switch to a static shaderc build.
- Public release builds are needed for compressed release game scripts. Keep
  development builds for source/debug layouts.
- The last known-good public SDL3 executable for Umineko Project was copied to
  both `D:\Umineko Project\onscripter-ru.exe` and
  `D:\Umineko Project\onscripter-ru-sdl3-public.exe`.

## Remaining Work

1. Finish the renderer cutover decision: keep SDL2_gpu as fallback for one more
   release or remove it after broader SDL3 regression coverage.
2. Run longer in-game visual regression passes across effects, transitions,
   subtitles, video, save/load screens, and resolution/fullscreen changes.
3. Run longer audio regression passes for BGM/SE/voice mixing, looping, fades,
   device changes, and pause/resume behavior under SDL3_mixer.
4. Port or translate any non-trivial legacy OpenGL GLSL that falls outside the
   current simple SDL2_gpu-style translator.
5. Remove SDL2_gpu, libepoxy, legacy GL/GLES backend files, and old ANGLE/DLL
   handling after SDL3 becomes the default renderer.
6. Replace Android packaging with a Gradle/AGP, aapt2, apksigner, scoped-storage,
   and modern manifest flow.
7. Evaluate libjpeg-turbo, Lua, libusb, Android libunwind removal, and removal of
   legacy custom libc++/libc++abi packages.

## References

- Main compilation guide: `Resources/Docs/Compilation.md`
- SDL3 GPU API: https://wiki.libsdl.org/SDL3/CategoryGPU
- SDL release archive: https://www.libsdl.org/release/
- SDL_image release archive: https://www.libsdl.org/projects/SDL_image/release/
- SDL_mixer release archive: https://www.libsdl.org/projects/SDL_mixer/release/
