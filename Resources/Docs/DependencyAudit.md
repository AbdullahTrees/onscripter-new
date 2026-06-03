# Dependency Audit and Modernization Plan

Date: 2026-05-31
Updated: 2026-06-03

This document is the current dependency and renderer modernization status for
`onscripter-new`. It is intentionally kept as a clean status record, not a
complete investigation log.

## Current Status

- The supported platform floors have been raised and encoded in the build
  scripts, packaging scripts, and docs.
- The low-risk compression/media leaf libraries, text stack, FFmpeg, and SDL3
  stack have been updated.
- smpeg2 has been removed from package metadata, engine includes, configure
  metadata, and Xcode references.
- SDL3, SDL3_image, and SDL3_mixer are the configure renderer, image, and mixer
  stack.
- SDL3 has replaced SDL2/SDL2_gpu as the only configure renderer path. The
  `--sdl2-renderer` fallback now exits with an error.
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
| SDL2 | 2.32.10 | Retired from the engine dependency graph after the SDL3 cutover; package recipe remains for historical reference. |
| SDL2_image | 2.8.12 | Retired from the engine dependency graph after the SDL3 cutover; package recipe remains for historical reference. |
| SDL2_mixer | 2.8.2 | Retired from the engine dependency graph after the SDL3 cutover; package recipe remains for historical reference. |
| SDL2_gpu | removed | Fallback renderer package recipe, patch, configure links, backend source references, and Xcode references removed. |
| libepoxy | removed | Temporary SDL2_gpu dependency package recipe and Xcode references removed. |
| SDL3 | 3.4.10 | Default renderer stack; built static with SDL_GPU and SDL renderer support enabled. |
| SDL3_image | 3.4.4 | Default image stack; PNG/JPEG enabled through local dependencies. |
| SDL3_mixer | 3.2.2 | Default mixer stack; WAV, MP3/dr_mp3, and Ogg Vorbis/libvorbisfile enabled. |
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
- Windows helper packaging now points at MSYS2 UCRT64. Old ANGLE, EGL/GLES, and
  d3dcompiler renderer DLL payloads are no longer part of the Windows package.

Dependency modernization:

- Low-risk bzip2, zlib, libpng, libogg, and libvorbis updates are complete.
- FreeType, HarfBuzz, FriBidi, and libass were updated together, with obsolete
  local patches removed or replaced by configure/cache options.
- FFmpeg was updated to 7.1.4 and the engine decode path was ported away from
  removed/deprecated FFmpeg 3.x APIs.
- SDL2, SDL2_image, and SDL2_mixer were updated for the fallback window and are
  now retired from the engine dependency graph.
- SDL2_gpu and libepoxy were removed after the fallback window, along with the
  SDL2_gpu package patch, legacy GL/GLES renderer source files, and old
  ANGLE/d3dcompiler Windows package payload.

SDL3 source path:

- The configure path selects SDL3, SDL3_image, and SDL3_mixer. The retired
  `--sdl2-renderer` fallback now exits with an error instead of building
  SDL2_gpu.
- SDL compatibility headers now isolate most SDL2/SDL3 API differences:
  initialization returns, surface formats, palette setup, color keys, image
  loading/saving, mixer APIs, syswm access, events, window state, keyboard,
  mouse, touch, joystick, cursor, and text input.
- `Support/SDLMixerCompat.cpp` maps the engine's current SDL2_mixer-style calls
  onto SDL3_mixer's mixer/track/audio model for the call surface used by the
  engine.
- The SDL3_mixer compatibility boundary now has high-resolution
  `Mix_VolumeFloat()` and `Mix_VolumeMusicFloat()` wrappers. Dynamic BGM and
  mix-channel property fades preserve fractional interpolation into
  SDL3_mixer's float `MIX_SetTrackGain()` path; SDL2 fallback builds still round
  through the legacy integer volume API.
- The SDL3_mixer adapter now keeps exact float channel/music volume state and
  reapplies track gain immediately after `MIX_PlayTrack()`. Explicit `dwave`
  stop/restart commands also clear queued mix-channel volume properties so a
  stale `ach_prop` fade cannot keep driving a newly reused channel, including
  the project-logo/Witch Hunt wind effect on channel 16.
- A follow-up Witch Hunt splash investigation confirmed the wind volume dip was
  also present in the SDL2 fallback build. The shared `dwave` command path now
  detects the immediate `dwave*` followed by timed same-channel `ach_prop`
  pattern and primes that channel to zero before playback, avoiding a fade from
  stale legacy engine SFX volume to the script's `%sfx_vol`.
- `Engine/Graphics/SDL3GPUCompat.cpp` provides the SDL3_GPU transition backend:
  texture/render-target ownership, CPU backing storage, uploads, presentation,
  native fixed-pipeline blits, `GPU_TriangleBatch`, render-target readback, and
  PNG screenshot output.
- Built-in project shader effects attempt native SDL_shadercross compilation in
  shadercross builds, with the SDL3 CPU compatibility evaluator retained as a
  fallback when translation or native drawing cannot execute safely.
- The common transition blend shader has embedded precompiled SPIR-V for the
  SDL3_GPU Vulkan path, avoiding runtime shaderc for the transition hot path.
- Optional SDL_shadercross support can run external SPIR-V/HLSL shaders through
  native SDL_GPU pipelines. Runtime shaderc support is opt-in for modern
  Vulkan-layout GLSL that needs runtime GLSL-to-SPIR-V compilation.
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
- SDL3_GPU fixed-pipeline blits now batch compatible draws and flush before
  CPU-visible operations, presentation, shader state changes, and image
  destruction. Full clears use lazy solid-color CPU mirrors, and render-target
  surface copy-out can avoid synchronizing the persistent CPU mirror while
  readback reuses a grow-only download transfer buffer.
- Recognized built-in SDL2_gpu-style fragment shaders now attempt native
  SDL_shadercross execution before falling back to the CPU shader evaluator.
  The legacy translator handles comma-separated uniform declarations,
  texture-coordinate swizzles, and integer-backed boolean uniforms, which lets
  common transition shaders such as `blendByMask.frag` run as native SDL_GPU
  fragment programs in shadercross builds.
- `blendByMask.frag`, `blurH.frag`, `blurV.frag`, and
  `colourConversion.frag` now have embedded precompiled SPIR-V and are selected
  before shadercross, so common mask/crossfade transitions, logo blur
  transitions, and YUV video conversion can run natively without linking
  shaderc.
- `glyphGradient.frag` and `colorModification.frag` now use the same embedded
  precompiled SPIR-V path as the other hot built-in shaders, covering the
  measured text/glyph CPU fallback shaders without requiring runtime shaderc.
- `alphaOutsideTextures.frag` now has embedded precompiled SPIR-V for the SDL3
  breakup triangle path. A later in-game telemetry point showed this shader as
  the only CPU shader fallback source, with 357 fallback draws and 70.65 MP
  evaluated; the new native shader path is build-verified and needs follow-up
  runtime telemetry.
- The remaining built-in fragment shaders now also have embedded SPIR-V:
  `breakup.frag`, `cropByMask.frag`, `effectTrvswave.frag`,
  `effectWarp.frag`, `effectWhirl.frag`, `glassSmash.frag`,
  `mergeAlpha.frag`, `multiplyAlpha.frag`, `pixelate.frag`,
  `renderSubtitles.frag`, and `textFade.frag`. The static
  `Resources/Shaders/*.frag` inventory now has no built-in fragment shader
  without native Vulkan bytecode.
- Native SDL3 shader draws now map sampler bindings to legacy image units when
  needed. `renderSubtitles.frag` uses that mapping to sample the subtitle atlas
  natively while rendering into the current subtitle frame, without weakening
  the render-to-self guard for shaders that actually sample their render target.
- Compatible fixed-pipeline `GPU_TriangleBatch` draws and native shader-program
  indexed draws now queue through a native triangle batch instead of submitting
  one SDL_GPU command buffer per call. The batch key includes target, pipeline,
  samplers, viewport, scissor, blend state, shader kind, and native uniform
  registers; switching between queued blits and queued triangles flushes the
  previous queue to preserve draw order.
- `configure` now selects SDL3/SDL3_image/SDL3_mixer and the SDL3_GPU renderer
  unconditionally. The `--sdl2-renderer` option is retained only to print a
  removal error for stale scripts.
- SDL3_GPU renderer telemetry can be enabled with `--sdl3-gpu-telemetry` or
  `ONS_SDL3_GPU_TELEMETRY=1`. On renderer shutdown it logs aggregate command
  buffer, texture upload, readback, native draw, CPU blit, CPU shader fallback,
  per-source texture upload/readback buckets, and per-shader native/fallback
  counters.
- The SDL3_GPU transfer telemetry now separates upload/readback traffic by
  source, including video frame uploads, subtitle uploads, surface copy-out,
  glyph atlas `simulateRead()`, CPU fallback paths, image copy/update paths,
  clears, and multiply-alpha updates.
- Full-clear upload fallback telemetry is split by cause and dimensions:
  full-image native-clear failures use `clear_full_native_fallback_<width>x<height>`,
  while clipped/virtual-resolution clears use
  `clear_full_clipped_upload_<target>_rect_<rect>`.
- Clipped `GPU_ClearRGBA()` calls now use a native solid-rectangle draw for
  the clear bounds instead of synchronizing the whole CPU mirror and uploading
  the clear rectangle. The fallback path remains available and is source-tagged
  under the split clear labels.
- Glyph atlas `simulateRead()` now creates its temporary atlas copy with a
  GPU-to-GPU blit instead of `GPU_CopyImage()`, avoiding an unnecessary
  CPU-mirror synchronization when the temporary image is first created.
- SDL3_GPU images that own native textures are now tracked in a backend live
  set. `GPU_FreeImage()`, target backing resize, mipmap recreation, and
  `GPU_Quit()` all release through the same helper, and shutdown releases any
  remaining live image textures before destroying the SDL_GPU device. The
  2026-06-02 shutdown-lifetime telemetry run no longer reported the Vulkan
  `VkImage`/`VkImageView` validation leaks that appeared in previous runs.
- The SDL3 PNG save path now keeps libpng `setjmp` inside a small write helper
  after surface conversion, locking, and row setup. The local UCRT64 rebuild no
  longer emits the previous `-Wclobbered` longjmp warnings from
  `saveSurfacePNG_RW()`.

## SDL3 Default Cutover Plan

1. [x] Make SDL3 the default renderer path while keeping SDL2_gpu available as
   an explicit fallback for the completed fallback window.
2. [x] Add lightweight renderer telemetry for native shader success/fallbacks,
   CPU shader pixels, texture uploads, readbacks, per-source transfer buckets,
   command buffers, and rendered native vertices.
3. [x] Run broader visual/audio regression across save/load UI, backlog, menus,
   fullscreen/resolution changes, subtitles, videos, major transitions, audio
   fades/loops, and device-change/pause-resume cases.
4. [x] Update any remaining packaging/docs/build defaults so the SDL3
   static-link path is treated as the normal Windows release path.
5. [x] Remove SDL2_gpu, libepoxy, legacy GL/GLES backend files, and old
   ANGLE/DLL handling after the fallback release window.

## Validation Snapshot

Dependency and build verification completed during this modernization pass:

- Historical native UCRT64 configure/build checks for the previous default SDL2
  stack.
- Clean package builds for the low-risk leaf dependencies, text stack, FFmpeg
  7.1.4, the retired SDL2 fallback stack, and the SDL3 default stack.
- Static link smoke tests for FFmpeg, libass, the retired SDL2_image/SDL2_mixer
  fallback packages, and related package metadata.
- SDL3 header and syntax probes for compatibility headers, renderer code, event
  code, window/input code, audio code, image paths, and SDL_mixer adapter paths.
- SDL3 renderer runtime probes for native blits, triangle batches, render-target
  readback, PNG screenshot signature validation, presentation, built-in shader
  compatibility, SPIR-V/HLSL shadercross paths, shaderc-backed GLSL, and simple
  legacy fragment GLSL translation.
- SDL3_mixer runtime probes for audio open/query, channel allocation, channel
  playback/halt callbacks, looping state, raw-stream effects, WAV music loading,
  music playback, and music finished callbacks. The precise SDL3 track-gain
  fade path and channel restart gain-property cleanup are build-verified;
  subjective listening validation is still pending.

Full game runtime validation:

- Built a Windows SDL3 public release with:

  ```
  ./configure --release-build --strip-binary --std=gnu++14
  make -j8
  ```

- Installed the result as `D:\Umineko Project\onscripter-ru.exe`.
- Booted Umineko Project release data with `en.file` through the SDL3 build.
- The game reached and remained at the main menu for a 120 second validation
  run.
- The process stayed responsive; no new Windows Application Error was produced.
- The runtime log showed SDL3_GPU initialization, shader compilation, SDL3_mixer
  audio startup, layer effect setup, and speech-level activity.

SDL3 renderer benchmark validation:

- The SDL3 public-release executable now supports `--sdl3-benchmark`.
- On the local Windows UCRT64 SDL3 build, 300 forced immediate 256x256 blits
  measured 108.376 ms total, while the compatible batched path measured
  0.255 ms total for enqueue plus one flush.
- Full-target clear submit cost measured 9.117 ms total for 300 clears; full
  clear surface copy-out remains measurable at 264.915 ms total because it
  includes a full destination surface fill/copy.

SDL3 source-tagged runtime telemetry:

- A user-controlled Umineko Project startup/video telemetry run with
  `--use-logfile --sdl3-gpu-telemetry` closed normally and flushed
  source-tagged transfer counters.
- The run recorded 28.06 GB of readbacks and 4.32 GB of texture uploads.
  CPU shader fallback accounted for 23.62 GB of readbacks, while `clear_full`
  accounted for 3.04 GB of uploads.
- YUV420P video plane uploads were visible as their own bucket at 1.02 GB and
  are no longer the largest measured transfer source.
- The follow-up telemetry target at that point was verifying that
  `glyphGradient.frag` and `colorModification.frag` reported native draws
  instead of CPU fallback draws, and using the split full-clear labels to locate
  the largest clear-upload fallback sizes.
- A follow-up telemetry run after embedding `glyphGradient.frag` and
  `colorModification.frag` verified zero CPU shader fallback draws/pixels in
  the same measured boot/video path. Those shaders compiled natively once each
  and drew 518 and 186 times respectively.
- The same follow-up run reduced readback traffic from 28.06 GB to 3.81 GB.
  Remaining readbacks were 359 full 2172x1222 RGBA synchronizations in the
  generic `ensure_pixels_current` bucket. Split clear telemetry showed only
  clipped full-clear uploads, dominated by 302 uploads of a 1920x1080 rectangle
  inside a 2172x1222 target.
- A follow-up run after native clipped clears recorded zero readbacks, zero CPU
  shader fallback draws/pixels, and zero clear fallback uploads. Texture upload
  volume fell to 1.14 GB, now limited to YUV420P video uploads, image updates,
  and multiply-alpha updates in the measured boot/video path.
- A shutdown-lifetime verification telemetry run started at 2026-06-02
  18:47:35 and closed normally after roughly 56 seconds. It recorded zero CPU
  blit/shader fallback, four 1920x1080 RGBA readbacks totaling 33.18 MB, and
  918.65 MB of uploads dominated by `update_image` and `multiply_alpha`.
  Neither `out.txt` nor `err.txt` contained the previous Vulkan
  `VkImage`/`VkImageView` validation leak output at shutdown.
- A later user-controlled in-game telemetry run at 2026-06-02 19:03:21
  identified `alphaOutsideTextures.frag` as the only CPU shader fallback:
  357 draws, 70.65 MP evaluated, and 3.90 GB of paired fallback readback/upload
  traffic. The shader now has an embedded SPIR-V path, the Windows public
  release binary was rebuilt, and the updated `onscripter-ru.exe` was copied to
  `D:\Umineko Project`.
- The 2026-06-03 UCRT64 build after embedding SPIR-V for every built-in
  fragment shader under `Resources/Shaders` linked successfully. The updated
  executable was copied to `D:\Umineko Project\onscripter-ru.exe`.
- The 2026-06-03 UCRT64 build after adding native indexed triangle/shader draw
  batching linked successfully. The updated executable was copied to
  `D:\Umineko Project\onscripter-ru.exe`.
- A follow-up 2026-06-03 UCRT64 build added `--sdl3-benchmark-output <path>`
  for captureable benchmark CSV output from the Windows GUI-subsystem binary,
  linked successfully, and was copied to
  `D:\Umineko Project\onscripter-ru.exe`. The post-batching benchmark reduced
  `triangle_batch_1024_quads_submit` from 174.628 us to 59.980 us average per
  iteration. A user-controlled startup/video telemetry run after the same build
  recorded zero readbacks and zero CPU blit/shader fallback pixels.
- The 2026-06-03 UCRT64 build after the shared `dwave`/`ach_prop` fade-priming
  fix linked successfully and was copied to
  `D:\Umineko Project\onscripter-ru.exe`. This addresses the Witch Hunt splash
  wind volume dip observed in both SDL2 and SDL3 builds by starting immediate
  timed same-channel volume fades from silence instead of stale engine SFX
  volume.
- The 2026-06-03 UCRT64 build after completing SDL3 cutover tasks 4 and 5
  linked successfully and was copied to
  `D:\Umineko Project\onscripter-ru.exe`. This build uses the SDL3-only
  configure path after removing SDL2_gpu/libepoxy, legacy GL/GLES backend
  sources, and old ANGLE/d3dcompiler Windows package payloads.
- The follow-up 2026-06-03 UCRT64 warning-clean release rebuild linked without
  any `warning:` lines and was copied to
  `D:\Umineko Project\onscripter-ru.exe`. The cleanup covered SDL3 joystick ID
  validation, desktop display-mode fallback initialization, SDL3-disabled touch
  gesture code, PNG load longjmp state, and an unused video framerate counter.
- The root `README.md` now describes `onscripter-new` as the Umineko Project
  ONScripter-RU modernization branch, points at the local compilation,
  dependency, and SDL3 performance docs, and reflects the current SDL3-only
  active renderer/dependency stack.

## Packaging Notes

- The current Windows SDL3 build links SDL3, SDL3_image, and SDL3_mixer
  statically. A normal SDL3 package does not need an SDL DLL folder beside the
  executable.
- Old ANGLE, EGL/GLES, and d3dcompiler renderer DLL payloads have been removed.
  The remaining `Resources/Windows/dlls` files are optional crash-reporting
  helpers only.
- Runtime shaderc support is not enabled by default. If
  `--sdl3-runtime-shaderc` is used with MSYS2's `shaderc.pc`, packaging must
  include `libshaderc_shared.dll` or switch to a static shaderc build.
- Public release builds are needed for compressed release game scripts. Keep
  development builds for source/debug layouts.
- The last known-good public SDL3 executable for Umineko Project was copied to
  both `D:\Umineko Project\onscripter-ru.exe` and
  `D:\Umineko Project\onscripter-ru-sdl3-public.exe`.

## Remaining Work

1. Run longer in-game visual regression passes across effects, transitions,
   subtitles, video, save/load screens, and resolution/fullscreen changes.
2. Run longer audio regression passes for BGM/SE/voice mixing, looping, fades,
   device changes, and pause/resume behavior under SDL3_mixer, including a
   fade-heavy listening pass after the high-resolution gain and shared
   `dwave`/`ach_prop` fade-priming changes.
3. Add more source attribution for `ensure_pixels_current` callers so future
   readbacks are not left in a generic bucket.
4. Use source-tagged SDL3_GPU transfer telemetry from representative
   playthroughs to shrink the largest remaining readback/upload sources.
5. Continue monitoring Vulkan validation output during longer SDL3_GPU
   playthroughs; the latest shutdown-lifetime verification was clean.
6. Follow up with representative shader telemetry at breakup-heavy, subtitle,
   pixelation, warp, whirl, old-breakup, and text-fade points to verify the
   newly embedded built-in SPIR-V paths report native draws with zero CPU
   fallback.
7. Use SDL3_GPU shader telemetry from representative playthroughs to identify
   any external shader, render-to-self safety case, or backend-specific native
   shader failure that still falls back to the CPU.
8. Use representative effect-heavy telemetry to verify the native indexed
   triangle/shader draw batching impact outside the synthetic benchmark.
9. Port or translate any non-trivial external OpenGL GLSL that falls outside the
   current simple SDL2_gpu-style translator.
10. Replace Android packaging with a Gradle/AGP, aapt2, apksigner, scoped-storage,
   and modern manifest flow.
11. Evaluate libjpeg-turbo, Lua, libusb, Android libunwind removal, and removal of
   legacy custom libc++/libc++abi packages.

## References

- Main compilation guide: `Resources/Docs/Compilation.md`
- SDL3 GPU API: https://wiki.libsdl.org/SDL3/CategoryGPU
- SDL release archive: https://www.libsdl.org/release/
- SDL_image release archive: https://www.libsdl.org/projects/SDL_image/release/
- SDL_mixer release archive: https://www.libsdl.org/projects/SDL_mixer/release/
