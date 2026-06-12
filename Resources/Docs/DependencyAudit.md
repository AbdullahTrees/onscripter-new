# Dependency Audit and Modernization Plan

Date: 2026-05-31
Updated: 2026-06-12

This document is the current dependency and renderer modernization status for
`onscripter-new`. It is intentionally kept as a clean status record, not a
complete investigation log.

## Current Status

- The supported platform floors have been raised and encoded in the build
  scripts, packaging scripts, and docs.
- The active C++ language floor has been raised to C++23 for configure/make,
  Xcode project settings, clang-tidy, and the packed-script helper tools.
- The old macOS/iOS floor cleanup is complete: unsupported Apple Xcode targets,
  the Snow Leopard-era macOS reloader wrapper, custom MacPorts clang project
  metadata, ARMv7 helper wrappers, and active local static libc++/libc++abi
  archive links have been removed.
- The legacy libc++/libc++abi package recipes and Snow Leopard patch have been
  removed now that current Apple build paths use Xcode's SDK/runtime libc++.
- The low-risk compression/media leaf libraries, text stack, FFmpeg, SDL3
  stack, Lua, libusb, and JPEG provider have been updated.
- smpeg2 has been removed from package metadata, engine includes, configure
  metadata, and Xcode references.
- SDL3, SDL3_image, and SDL3_mixer are the configure renderer, image, and mixer
  stack.
- SDL3 has replaced SDL2/SDL2_gpu as the only configure renderer path. The
  `--sdl2-renderer` fallback now exits with an error.
- The SDL3 Windows public-release executable has booted Umineko Project release
  data to the main menu with SDL3_GPU rendering and SDL3_mixer audio active.
- The SDL3 main-menu resource pass reduced Umineko Project menu memory from the
  previous multi-gigabyte private/working-set spike to sub-1 GB steady-state
  samples, with Task Manager working-set observations around 410-526 MB and
  steady CPU samples around 4-4.5% while the menu video was visible.
- The 2026-06-04 video decode resource pass removes the per-frame YUV/NV12
  plane heap copy on the direct shader-conversion path, reduces decoded
  ready-frame buffering independently from packet buffering, avoids RGB staging
  surface preallocation when direct YUV conversion is selected, and frees
  transient video plane/mask GPU images at playback teardown.
- The follow-up 2026-06-04 CPU audit pass reduces steady-state allocation and
  lookup overhead in the event queue, temporary image/loader pools,
  SDL3 texture uploads, shader program activation, idle event-thread wakeups,
  and the video audio bridge callback.
- The follow-up 2026-06-04 transition-smoothness correction restores the SDL
  event fetcher's previous 8 ms idle cadence and restores clear-on-return for
  temporary GPU images after choppy transitions were observed, while preserving
  the lower-risk CPU audit changes.
- The next 2026-06-04 black-transition correction restores the temporary GPU
  image pool's previous unordered-map scan reuse policy after black transitions
  remained choppy, avoiding immediate LIFO reuse of full-screen pooled render
  targets while keeping CPU-side pool free lists.
- The 2026-06-04 audio backend audit reduces SDL3_mixer adapter allocation and
  gain-update overhead, removes 1 ms polling from non-event threaded sound
  loads, and avoids a decoded-frame copy for video audio that already matches
  the mixer format. No audio compression techniques were added.
- The 2026-06-04 text/sprite rendering audit removes allocation and lookup
  overhead in dialogue rendering, sprite z-level setup, transformed sprite
  canvas allocation, and fully transparent/fully opaque draw-state paths while
  preserving existing draw order and playback quality.
- The 2026-06-04 branding pass changes the default executable name to
  `onscripter-new.exe`, pins the Umineko Rondo window title without appended
  engine version text, and exposes the SDL3_GPU Vulkan backend as `Vulkan` in
  renderer selection UI/config while accepting legacy `SDL3_GPU` values.
- The 2026-06-04 menu responsiveness pass adds no new third-party dependencies.
  It uses existing script UI primitives, the existing packed-script tools,
  SDL display-mode refresh-rate queries, async image cache support, and the
  existing SDL3_mixer-backed music path.
- The follow-up 2026-06-04 animation-pacing pass adds no new third-party
  dependencies. It uses SDL's existing high-resolution performance counter,
  the existing clock/dynamic-property infrastructure, and a constrained
  renderer path for the packed text cursor sprite sheets.
- The 2026-06-05 animation follow-up adds no new third-party dependencies. It
  keeps the cursor smoothing in the renderer and changes the title Tips submenu
  script animation to existing dynamic sprite-property commands.
- The second 2026-06-05 animation follow-up adds no new third-party
  dependencies. It corrects cursor smoothing against the existing packed cursor
  sheet and moves the remaining title-menu button open animations to existing
  dynamic sprite-property commands.
- The third 2026-06-05 animation follow-up adds no new third-party
  dependencies. It adds a narrow engine-side grouped sprite-range dynamic
  property command and uses the existing packed-script tooling for the Config
  page scroll update.
- The 2026-06-05 Config reset UI text/layout pass adds no new third-party
  dependencies. It changes only existing packed-script aliases and reset-dialog
  positioning logic.
- The follow-up reset-choice color pass adds no new third-party dependencies.
  It changes only the existing packed-script choice aliases to match the Config
  button white/red hover scheme.
- The follow-up Config polish pass adds no new third-party dependencies. It
  changes only packed-script setting labels, slider sprite positions, and a
  textbox preview built from the existing `msgwnd` window assets.
- The immediate Config scroll-regression correction adds no new third-party
  dependencies. It changes only the textbox preview sprite IDs and removes the
  extra packed-script range-animation calls so Config page scrolling again uses
  one existing `spriterangept` range.
- The textbox selector layout follow-up adds no new third-party dependencies.
  It changes only packed-script Config row layout and button wiring so the
  existing textbox-window assets are previewed through left/right arrows.
- The textbox preview spacing/assets follow-up adds no new third-party
  dependencies. It changes packed-script Config preview coordinates and adds
  preview-only cropped PNGs derived from the existing textbox-window assets in
  the active game directory.
- The final textbox preview arrow alignment pass adds no new third-party
  dependencies. It changes only packed-script Config arrow coordinates.
- The empty textbox preview label follow-up adds no new third-party
  dependencies. It changes only packed-script Config preview text for the
  no-window textbox style.
- The `No Window` alignment follow-up adds no new third-party dependencies. It
  changes only the packed-script y coordinate for that preview label.
- The 2026-06-06 pause-menu UI pass adds no new third-party dependencies. It
  changes only packed-script pause-menu layout/text plus active game-directory
  pause-menu PNG artwork for the renamed `Hide` button and relocated
  episode/chapter labels.
- The immediate pause-menu hover/artwork follow-up adds no new third-party
  dependencies. It changes only packed-script button-cell reset logic in the
  pause-menu loop and active game-directory pause-menu PNG artwork.
- The second pause-menu follow-up adds no new third-party dependencies. It
  changes only packed-script pause-menu session-info text sizing, then restores
  the active game-directory `Clear` button PNG from its saved original-artwork
  backup.
- The emergency pause-menu input correction adds no new third-party
  dependencies. It restores the packed-script async button-wait loop after a
  failed script-side no-result rebuild attempt blocked button input.
- The Config input-hints pass adds no new third-party dependencies. It changes
  only the packed English script so the old `DualShock` option is labeled
  `Gamepad`, the row is labeled `Input Hints`, and the Controls popup switches
  between keyboard/mouse and gamepad binding text according to the saved
  `control_interface` setting.
- The pause-menu hover root-cause follow-up adds no new third-party
  dependencies. It changes only packed-script session-info sprite allocation
  and update logic so the live pause-menu polling loop no longer clears normal
  sprites with `_csp` while button hover tracking is active.
- The session-label follow-up adds no new third-party dependencies. It changes
  only packed-script pause-menu text for the no-active-track display value.
- The Tips/Characters detail follow-up adds no new third-party dependencies. It
  changes only packed-script menu positioning and string-button selected-state
  logic for the Tips and Characters menus.
- The 2026-06-06 whole-codebase performance audit adds no new third-party
  dependencies. It uses the existing SDL3_GPU and FFmpeg APIs, including fixed
  native sampler bindings for triangle batches and `av_frame_clone()` for
  retained hardware-converted video frames, while keeping game timing and
  visual/audio output quality unchanged.
- The 2026-06-09 medium whole-codebase performance pass adds no new third-party
  dependencies. It uses existing SDL semaphores and SDL3_GPU texture-copy,
  transfer-buffer, and native shader paths to reduce subtitle queue polling,
  CPU image mirrors, full-image alpha premultiplication loops, and mipmap
  readback/reupload work while leaving script timing math unchanged.
- The Config controls layout and polish follow-ups add no new third-party
  dependencies. They change only packed-script Config button positioning, the
  restart action label, and the modal controls/keybind reference styling and
  alignment, including the separated centered popup header and removed Backlog
  section. The latest correction also keeps the popup body text in the existing
  foreground sprite range so the dim overlay does not occlude it, pins the
  fixed-width listing box to the canvas center, and scopes the width tag around
  the full popup body so every listing line uses that same centered wrap area.
- The 2026-06-10 Trophies UI polish pass adds no new third-party dependencies.
  It changes only packed-script trophy text aliases so unlocked entries show
  the trophy name and colored rarity parenthetical on one line, followed by the
  description.
- The same-date Trophies scrollbar follow-up adds no third-party dependencies.
  It changes only shared engine input handling for special scrollables so a
  visible scrollbar thumb can be left-click dragged without producing a
  script-visible button result. The corrected input path captures the raw
  left mouse-down before normal `btndown` filtering so `btnwait2` menus can
  start thumb drags without enabling script-visible button-down reporting.
- The same-date title-loading/title-caption follow-up adds no third-party
  dependencies. It uses the existing async cache queues, SDL semaphores, and
  wait-event frame pump so the title loading animation remains active while
  queued cache prewarm work drains. It also keeps the window-title hardening by
  mapping recognized Rondo/Chiru captions to pinned titles instead of accepting
  arbitrary script caption text.
- The 2026-06-11 miscellaneous UI/text/title pass adds no third-party
  dependencies. It changes only packed-script trophy/title text and the pinned
  Chiru window title string; an immediate follow-up restores the prior Config
  coordinate values.
- The 2026-06-12 release-audit cleanup adds no third-party dependencies. It
  fixes external IDE helper scripts to copy the current `onscripter-new`
  executable name, removes the stale ignore rule for the tracked dependency
  audit document, collapses two byte-identical macOS app-icon PNGs by reusing
  the existing same-size asset files, tightens Android APK packaging to require
  current `onscripter-new` engine outputs, removes stale async comments, and
  updates current benchmark/telemetry examples to the renamed executable. It
  also removes an unused sprite-helper method and trims a dead parameter from
  the remaining sprite helper API. The broader verification follow-up keeps the
  dependency set unchanged while fixing Android cross-build portability in the
  joystick libusb guard, standard-library includes, Droid profiler atomic stop
  path, and SDL2-only touch-threshold declarations.

## Support Floor

| Platform | Support floor | Notes |
| --- | --- | --- |
| Windows | Windows 10 | Prefer x86_64 UCRT64 builds. `_WIN32_WINNT` and `WINVER` are set to `0x0A00`. |
| macOS | macOS 14 | x86_64 remains supported; arm64 is the target for Apple Silicon work. Legacy i386, x86_64h, custom clang, and local static libc++ archive support is removed from the active floor. |
| iOS | iOS 17 | arm64 devices and current simulator targets. armv7/armv7s Xcode targets and helper wrappers are removed. |
| Android | Android 14/API 34 | arm64-v8a is the default target; x86_64 is available for emulator and development builds. APK packaging targets Android 16/API 36. |
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
| FFmpeg | 7.1.4 | Engine ported to current send/receive decode APIs and `AVChannelLayout`. Windows D3D11VA/DXVA2/Media Foundation/CUDA LLVM probing is disabled in the package. Android builds use hidden visibility so static AArch64 FFmpeg objects can link into `libmain.so`. |
| SDL2 | 2.32.10 | Retired from the engine dependency graph after the SDL3 cutover; package recipe remains for historical reference. |
| SDL2_image | 2.8.12 | Retired from the engine dependency graph after the SDL3 cutover; package recipe remains for historical reference. |
| SDL2_mixer | 2.8.2 | Retired from the engine dependency graph after the SDL3 cutover; package recipe remains for historical reference. |
| SDL2_gpu | removed | Fallback renderer package recipe, patch, configure links, backend source references, and Xcode references removed. |
| libepoxy | removed | Temporary SDL2_gpu dependency package recipe and Xcode references removed. |
| SDL3 | 3.4.10 | Default renderer stack; built static with SDL_GPU and SDL renderer support enabled. |
| SDL3_image | 3.4.4 | Default image stack; PNG/JPEG enabled through local dependencies. |
| SDL3_mixer | 3.2.2 | Default mixer stack; WAV, MP3/dr_mp3, and Ogg Vorbis/libvorbisfile enabled. |
| smpeg2 | removed | Retired from dependency graph and build metadata. |
| jpeg | libjpeg-turbo 3.1.4.1 | Replaces IJG 9c while still providing the `jpeg` package and static `libjpeg` API for SDL3_image. Built with JPEG v8 API emulation and TurboJPEG API disabled. |
| Lua | 5.4.8 | Latest Lua 5.4 line; Windows recipe builds the static `liblua.a` archive instead of Lua's DLL-oriented MinGW target. |
| libusb | 1.0.30 | Updated from the official release tarball. The old MinGW GUID patch is obsolete because current libusb uses `DEFINE_GUID()`. |
| libunwind | removed | Android now relies on the modern NDK/runtime unwinder and the profiling code's header-gated fallback. |
| libc++/libc++abi | removed | Legacy 8.0.0 package recipes and the Snow Leopard patch were removed after the Apple floor cleanup. Current Apple builds use Xcode's SDK/runtime libc++. |

## Completed Work

Platform/build modernization:

- `configure` defaults Android builds to arm64, rejects unsupported Android
  architectures, detects Windows UCRT64/x86_64, and applies the Windows 10 API
  floor.
- `Dependencies/build.sh` defaults Apple deployment to macOS 14/iOS 17, rejects
  unsupported Apple 32-bit targets, defaults Android to modern arm64 toolchains,
  and recognizes Android x86_64.
- The 2026-06-09 C++23 migration updates the default configure standard to
  C++23, keeps Windows release builds on the GNU dialect through
  `--std=gnu++23`, updates Xcode and tidy settings, and moves the standalone
  `nscmake`/`nscdec` Makefiles from `c++0x` to `c++23`.
- The 2026-06-07 Apple floor cleanup narrowed onscrlib Apple SDK discovery to
  macOS 14+/iOS 17+ SDKs, rejects unsupported macOS `i386`/`x86_64h` and iOS
  `armv7`/`armv7s` selections, removes ARMv7 gas-preprocessor/compiler
  wrappers, and removes the old 32-bit branches from FFmpeg/libass/HarfBuzz
  package recipes.
- `Scripts/ndktoolchain.sh` targets NDK r29/API 34 and creates arm64/x86_64
  wrapper toolchains.
- Android packaging uses Gradle 9.4.1, Android Gradle Plugin 9.2.0, min SDK
  34, target SDK 36, AAPT2, D8, zipalign, apksigner, scoped app storage, and
  arm64-v8a/x86_64 packaging.
- Xcode project deployment targets were raised to macOS 14 and iOS 17. The
  follow-up floor cleanup removed unsupported osx32/osx64h, the
  Snow Leopard-era `onscripter-ru-osx`/`mac_reloader` wrapper, onscrlib32,
  onscrlib64h, armv7, and armv7s targets; the remaining Apple targets use
  Xcode's default clang and SDK-relative iOS framework references.
- Windows helper packaging now points at MSYS2 UCRT64. Old ANGLE, EGL/GLES, and
  d3dcompiler renderer DLL payloads are no longer part of the Windows package.

Dependency modernization:

- Low-risk bzip2, zlib, libpng, libogg, and libvorbis updates are complete.
- FreeType, HarfBuzz, FriBidi, and libass were updated together, with obsolete
  local patches removed or replaced by configure/cache options.
- FFmpeg was updated to 7.1.4 and the engine decode path was ported away from
  removed/deprecated FFmpeg 3.x APIs.
- IJG jpeg 9c was replaced by libjpeg-turbo 3.1.4.1. The local package remains
  named `jpeg` to preserve SDL_image/configure dependencies.
- Lua was updated from 5.3.5 to 5.4.8, with the Windows recipe changed to build
  the static archive expected by the engine dependency layout.
- libusb was updated from 1.0.22 to 1.0.30. The obsolete MinGW GUID patch was
  removed, and the recipe creates libusb's resource dependency directory before
  the Windows `windres` compile step.
- SDL2, SDL2_image, and SDL2_mixer were updated for the fallback window and are
  now retired from the engine dependency graph.
- SDL2_gpu and libepoxy were removed after the fallback window, along with the
  SDL2_gpu package patch, legacy GL/GLES renderer source files, and old
  ANGLE/d3dcompiler Windows package payload.
- The legacy libc++/libc++abi 8.0.0 package recipes and Snow Leopard patch were
  removed after the Apple floor cleanup confirmed no active build path depends
  on local static libc++ archives.

SDL3 source path:

- The configure path selects SDL3, SDL3_image, and SDL3_mixer. The retired
  `--sdl2-renderer` fallback now exits with an error instead of building
  SDL2_gpu.
- SDL compatibility headers now isolate most SDL2/SDL3 API differences:
  initialization returns, surface formats, palette setup, color keys, image
  loading/saving, mixer APIs, syswm access, events, window state, keyboard,
  mouse, touch, joystick, cursor, and text input.
- The 2026-06-09 controller support pass uses SDL3's built-in gamepad subsystem
  for mapped controllers, including live device add/remove handling, normalized
  DualShock 4-style button mapping, SDL gamepad rumble fallback, and higher
  analog-stick thresholds with hysteresis. No new dependency was added; raw
  SDL joystick and existing libusb/native rumble paths remain fallbacks.
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
- The text/sprite rendering pass skips fully transparent glyph draw calls,
  delays renderable glyph cache lookups until a text pass is known to draw,
  traverses dialogue piece/ruby deques directly during rendering and fade
  ticking, fills sprite z-levels without first materializing an ordered sprite
  set, delays temporary canvas checkout for transformed sprites and scaled
  big images until after early exits/clips, and avoids redundant full-opacity
  RGBA writes for big-image chunks.
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
- SDL3_GPU command-buffer submissions now go through a shared submit helper that
  records telemetry and periodically waits for the GPU after a small backlog of
  queued command buffers. This prevents the D3D12/SDL3_GPU private-memory spike
  seen during Umineko Project main-menu loading, where the process previously
  climbed above 6 GB while live engine textures and CPU mirrors were small.
- SDL3_GPU images now allocate CPU pixel mirrors lazily, discard clean mirrors
  after GPU upload when callers no longer need CPU access, and directly upload
  video/frame byte rows without retaining a persistent CPU copy. Redundant
  decoded surfaces are freed after their GPU image or big-image representation
  exists, and the decoded image cache now has a default 64 MiB budget
  configurable through `ONS_IMAGE_CACHE_MB`.
- Hardware video decoding and hardware frame conversion are enabled by default
  on all platforms unless explicitly disabled with `--hwdecoder off` or
  `--hwconvert off`. On the local SDL3 Windows menu profile this restored the
  YUV plane-upload path and lowered CPU compared with the temporary RGB surface
  conversion default.
- Direct YUV/NV12 video frames now keep a retained FFmpeg `AVFrame` reference
  for queued decoded frames and upload from those planes directly. This removes
  the previous `new[]`/`memcpy` copy of every decoded plane before the SDL3_GPU
  upload copy. Subtitle blending makes the retained frame writable only when
  subtitles are active.
- The decoded video ready-frame queue now uses a smaller frame-specific limit
  while compressed packet buffering and startup timecode sampling keep their
  previous depth. Direct shader-converted videos also skip preallocating RGB
  staging surfaces; software-converted fallback videos still allocate staging
  surfaces from the existing pool on demand.
- Media playback now releases transient YUV plane textures and alpha-mask
  helper textures when playback finishes, including cases where the final RGB
  frame is intentionally left visible. The SWS context teardown now clears the
  freed pointer to avoid stale reuse/double-free risk if a later frame falls
  back from direct conversion to software scaling.
- SDL event queues now store `SDL_Event` values instead of
  `std::unique_ptr<SDL_Event>`, and the event fetcher reuses stack event
  storage for normal SDL traffic. This removes per-event heap allocation in the
  Windows/Linux SDL event path while preserving the existing queue ordering and
  finger-event coalescing behavior.
- The async loop no longer posts an unused results semaphore for no-result
  queues such as the SDL event fetcher. The event fetcher idle timeout was
  briefly raised during the CPU audit, then restored to the previous 8 ms
  cadence during transition-smoothness follow-up.
- Temporary CPU surface and PNG loader pools now keep explicit free lists for
  O(1) checkout of reusable entries instead of linearly scanning their backing
  maps for an unused object. The temporary GPU image pool was restored to its
  previous unordered-map scan reuse policy and clear-on-return behavior after
  black transitions remained choppy with immediate free-list reuse.
- SDL3 texture uploads now use one contiguous `memcpy` when the source rows and
  transfer rows are tightly packed. Row-by-row upload with padding remains the
  fallback for pitches that require it.
- Repeated high-level shader activation now caches the last program alias
  pointer, avoiding repeated string construction/hash lookup for common
  frame-to-frame shader paths such as video conversion and transition effects.
- The video `AudioBridge` mixer callback now writes at `raw + rawPos` instead
  of advancing the output pointer by the cumulative position, avoiding
  unnecessary pointer churn and preserving correct packing when one mixer
  buffer is filled by multiple decoded chunks.
- The 2026-06-04 RAM audit lowers the default decoded image cache budget to
  64 MiB, keeps `ONS_IMAGE_CACHE_MB` as the override, rounds reusable SDL3_GPU
  staging buffers to 256 KiB alignment instead of the next power of two, and
  releases the synchronous readback transfer buffer immediately after copying
  the downloaded pixels.
- The audio backend audit keeps the SDL3_mixer compatibility API and playback
  quality unchanged while reducing hot-path overhead: `Mix_LoadWAV_RW()` now
  appends decoded PCM directly into its final SDL-managed buffer, normal
  one-shot `MIX_PlayTrack()` calls use default options without allocating an
  SDL properties object, unchanged channel/music gains no longer call back into
  SDL3_mixer, non-event threaded sound loads block on the result semaphore
  instead of polling every millisecond, and FFmpeg video-audio frames that
  already match the mixer spec are retained by reference instead of copied.

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
- The 2026-06-03 dependency refresh rebuilt libjpeg-turbo 3.1.4.1, SDL3_image
  3.4.4 against that JPEG provider, Lua 5.4.8, and libusb 1.0.30 during a local
  UCRT64 release configure/build. The build linked without `warning:` lines and
  the updated executable was copied to `D:\Umineko Project\onscripter-ru.exe`.
- The 2026-06-03 SDL3 main-menu resource build linked successfully and was
  copied to `D:\Umineko Project\onscripter-ru.exe`. Main-menu screenshots and
  CSV samples under `DerivedData\profile-submit-throttle` and
  `DerivedData\profile-final-default-hw-on` show the visible Umineko Project
  menu running below the original RAM/CPU targets after command-buffer
  back-pressure and hardware conversion defaults were applied.
- The 2026-06-04 video decode resource build linked successfully without
  warning output and was copied to `D:\Umineko Project\onscripter-ru.exe`.
  Benchmarks and runtime telemetry were intentionally not run for this pass.
- The follow-up 2026-06-04 CPU audit build linked successfully without warning
  output and was copied to `D:\Umineko Project\onscripter-ru.exe`. Benchmarks
  and runtime telemetry were intentionally not run for this pass.
- The follow-up 2026-06-04 transition-smoothness build linked successfully and
  was copied to `D:\Umineko Project\onscripter-ru.exe`. Benchmarks and runtime
  telemetry were intentionally not run for this pass.
- The follow-up 2026-06-04 black-transition build linked successfully and was
  copied to `D:\Umineko Project\onscripter-ru.exe`. Benchmarks and runtime
  telemetry were intentionally not run for this pass.
- The 2026-06-04 RAM audit build linked successfully and was copied to
  `D:\Umineko Project\onscripter-ru.exe`. Benchmarks and runtime telemetry were
  intentionally not run for this pass.
- The 2026-06-04 audio backend audit build linked successfully and was copied
  to `D:\Umineko Project\onscripter-ru.exe`. Benchmarks and runtime telemetry
  were intentionally not run for this pass.
- The 2026-06-04 menu responsiveness build linked successfully and was copied
  to `D:\Umineko Project\onscripter-new.exe`. The repacked active English
  script was copied to `D:\Umineko Project\en.file` after an exact decode
  round-trip check. Benchmarks and runtime telemetry were intentionally not run
  for this pass.
- The follow-up 2026-06-04 animation-pacing build linked successfully and was
  copied to `D:\Umineko Project\onscripter-new.exe`. Benchmarks and runtime
  telemetry were intentionally not run for this pass.
- The 2026-06-05 animation follow-up build linked successfully. The active
  English script was repacked and decode round-trip verified, then the updated
  executable and `en.file` were copied to `D:\Umineko Project`. Benchmarks and
  runtime telemetry were intentionally not run for this pass.
- The second 2026-06-05 animation follow-up build linked successfully. The
  active English script was repacked and decode round-trip verified, then the
  updated executable and `en.file` were copied to `D:\Umineko Project`.
  Benchmarks and runtime telemetry were intentionally not run for this pass.
- The 2026-06-05 Config reset UI script update repacked the active English
  script and passed an exact decode round-trip check. `make -j8` reported the
  binary target was already current; the current `onscripter-new.exe` and
  updated `en.file` were copied to `D:\Umineko Project`. Benchmarks and runtime
  telemetry were intentionally not run for this script-only pass.
- The follow-up reset-choice color script update repacked the active English
  script and passed an exact decode round-trip check. `make -j8` again reported
  the binary target was already current; the current `onscripter-new.exe` and
  updated `en.file` were copied to `D:\Umineko Project`. Benchmarks and runtime
  telemetry were intentionally not run for this script-only pass.
- The follow-up Config polish script update repacked the active English script
  and passed an exact decode round-trip check. `make -j8` again reported the
  binary target was already current; the current `onscripter-new.exe` and
  updated `en.file` were copied to `D:\Umineko Project`. Benchmarks and runtime
  telemetry were intentionally not run for this script-only pass.
- The immediate Config scroll-regression correction repacked the active English
  script and passed an exact decode round-trip check. `make -j8` again reported
  the binary target was already current; the current `onscripter-new.exe` and
  fixed `en.file` were copied to `D:\Umineko Project`. Benchmarks and runtime
  telemetry were intentionally not run for this script-only pass.
- The textbox selector layout follow-up repacked the active English script and
  passed an exact decode round-trip check. `make -j8` again reported the binary
  target was already current; the current `onscripter-new.exe` and updated
  `en.file` were copied to `D:\Umineko Project`. Benchmarks and runtime
  telemetry were intentionally not run for this script-only pass.
- The textbox preview spacing/assets follow-up repacked the active English
  script and passed an exact decode round-trip check. `make -j8` again reported
  the binary target was already current; the current `onscripter-new.exe` and
  updated `en.file` were copied to `D:\Umineko Project`. Benchmarks and runtime
  telemetry were intentionally not run for this UI asset/layout pass.
- The final textbox preview arrow alignment pass repacked the active English
  script and passed an exact decode round-trip check. `make -j8` again reported
  the binary target was already current; the current `onscripter-new.exe` and
  updated `en.file` were copied to `D:\Umineko Project`. Benchmarks and runtime
  telemetry were intentionally not run for this coordinate-only pass.
- The empty textbox preview label follow-up repacked the active English script
  and passed an exact decode round-trip check. `make -j8` again reported the
  binary target was already current; the current `onscripter-new.exe` and
  updated `en.file` were copied to `D:\Umineko Project`. Benchmarks and runtime
  telemetry were intentionally not run for this script-only pass.
- The `No Window` alignment follow-up repacked the active English script and
  passed an exact decode round-trip check. `make -j8` again reported the binary
  target was already current; the current `onscripter-new.exe` and updated
  `en.file` were copied to `D:\Umineko Project`. Benchmarks and runtime
  telemetry were intentionally not run for this coordinate-only pass.
- The 2026-06-06 pause-menu UI pass repacked the active English script and
  passed an exact decode round-trip check. The UCRT64 release executable
  relinked successfully with no warning lines in the captured output, then the
  updated `onscripter-new.exe`, `en.file`, and active pause-menu artwork were
  copied to `D:\Umineko Project`. Benchmarks, runtime telemetry, and executable
  boot testing were intentionally not run for this UI/layout pass.
- The immediate pause-menu hover/artwork follow-up repacked the active English
  script and passed an exact decode round-trip check. `make -j8` reported the
  binary target was already current; the current `onscripter-new.exe`, updated
  `en.file`, and corrected active pause-menu artwork were copied to
  `D:\Umineko Project`. Benchmarks, runtime telemetry, and executable boot
  testing were intentionally not run for this UI/artwork correction.
- The second pause-menu follow-up repacked the active English script and passed
  an exact decode round-trip check. `make -j8` reported the binary target was
  already current; the current `onscripter-new.exe`, updated `en.file`, and
  restored original `Clear` button artwork were copied to
  `D:\Umineko Project`. Benchmarks, runtime telemetry, and executable boot
  testing were intentionally not run for this UI/script correction.
- The emergency pause-menu input correction repacked the active English script
  and passed an exact decode round-trip check. The UCRT64 release executable
  rebuilt successfully, then the updated `onscripter-new.exe` and `en.file`
  were copied to `D:\Umineko Project`. Benchmarks, runtime telemetry, and
  executable boot testing were intentionally not run for this input/hover
  correction.
- The pause-menu hover root-cause follow-up repacked the active English script
  and passed an exact decode round-trip check. The failed engine hover repaint
  experiments were reverted, the UCRT64 release executable rebuilt
  successfully, then the updated `onscripter-new.exe` and `en.file` were copied
  to `D:\Umineko Project`. Benchmarks, runtime telemetry, and executable boot
  testing were intentionally not run for this script/hover correction.
- The session-label follow-up repacked the active English script and passed an
  exact decode round-trip check. `make -j8` reported the binary target was
  already current; the current `onscripter-new.exe` and updated `en.file` were
  copied to `D:\Umineko Project`. Benchmarks, runtime telemetry, and executable
  boot testing were intentionally not run for this text-only correction.
- The Tips/Characters detail follow-up repacked the active English script and
  passed an exact decode round-trip check. `make -j8` reported the binary target
  was already current; the current `onscripter-new.exe` and updated `en.file`
  were copied to `D:\Umineko Project`. Benchmarks, runtime telemetry, and
  executable boot testing were intentionally not run for this packed-script UI
  correction.
- The 2026-06-06 whole-codebase performance audit completed three UCRT64
  `make -j8` rebuilds successfully after engine changes, then copied the
  rebuilt `onscripter-new.exe` to `D:\Umineko Project`. Benchmarks, runtime
  telemetry, and executable boot testing were intentionally not run for this
  source-level allocation/lookup pass.
- The Config controls layout follow-up repacked the active English script and
  passed an exact decode round-trip check. `make -j8` reported the binary target
  was already current; the current `onscripter-new.exe` and updated `en.file`
  were copied to `D:\Umineko Project`. Benchmarks, runtime telemetry, and
  executable boot testing were intentionally not run for this packed-script UI
  update.
- The Config controls polish follow-up repacked the active English script and
  passed another exact decode round-trip check after renaming the visible action
  to `Restart Game`, color-coding Controls popup keybind text, and centering the
  popup listing. `make -j8` again reported the binary target was already current;
  the current `onscripter-new.exe` and updated `en.file` were copied to
  `D:\Umineko Project`. Benchmarks, runtime telemetry, and executable boot
  testing were intentionally not run for this packed-script UI update.
- The immediate Controls popup correction repacked the active English script and
  passed another exact decode round-trip check after splitting the `Controls`
  header into a separate centered text sprite and removing the dedicated Backlog
  section from the listing. `make -j8` again reported the binary target was
  already current; the current `onscripter-new.exe` and updated `en.file` were
  copied to `D:\Umineko Project`. Benchmarks, runtime telemetry, and executable
  boot testing were intentionally not run for this packed-script UI update.
- The Controls popup layer correction repacked the active English script and
  passed another exact decode round-trip check after moving the listing body
  from sprite `6` to foreground sprite `3`, leaving the dim overlay on sprite
  `5` and the centered header on sprite `2`. `make -j8` again reported the
  binary target was already current; the current `onscripter-new.exe` and
  updated `en.file` were copied to `D:\Umineko Project`. Benchmarks, runtime
  telemetry, and executable boot testing were intentionally not run for this
  packed-script UI update.
- The Controls popup body-centering correction repacked the active English
  script and passed another exact decode round-trip check after pinning the
  fixed-width `1700px` listing box at `x=110` and wrapping the full popup body
  in the `{w:1700:...}` scope, ensuring every listing line uses the same
  centered wrap area. `make -j8` again reported the binary target was already
  current; the current `onscripter-new.exe` and updated `en.file` were copied
  to `D:\Umineko Project`. Benchmarks, runtime telemetry, and executable boot
  testing were intentionally not run for this packed-script UI update.
- The 2026-06-07 Apple floor cleanup removed the old macOS/iOS Xcode targets,
  Snow Leopard-era macOS reloader wrapper, ARMv7 helper wrappers, active local
  static libc++/libc++abi archive links, and obsolete Apple package recipe
  branches. The UCRT64 release rebuild linked successfully, then copied the
  rebuilt `onscripter-new.exe` to `D:\Umineko Project\onscripter-new.exe`.
  Benchmarks, runtime telemetry, and executable boot testing were intentionally
  not run for this dependency/build cleanup.
- The follow-up 2026-06-07 dependency removal deleted the legacy libc++ and
  libc++abi package recipes plus the Snow Leopard libc++ patch. `make -j8`
  reported the binary target was already current, then copied the current
  `onscripter-new.exe` to `D:\Umineko Project\onscripter-new.exe`. Benchmarks,
  runtime telemetry, and executable boot testing were intentionally not run for
  this package-removal cleanup.
- The 2026-06-09 C++23 migration rebuilt the local Windows/UCRT64 public
  release with `--std=gnu++23`, linked successfully with no warning output
  after replacing deprecated mixed-enum libusb request masks, and copied the
  rebuilt `onscripter-new.exe` to `D:\Umineko Project\onscripter-new.exe`.
  The helper `nscmake` and `nscdec` tools were force-rebuilt with `-std=c++23`.
  Benchmarks, runtime telemetry, and executable boot testing were intentionally
  not run for this language-level/source-cleanup pass.
- The 2026-06-09 controller support pass rebuilt the local Windows/UCRT64
  target after adding SDL3 gamepad hotplug/input routing, DualShock 4-style
  normalized button mapping, known Sony vendor/product GUID normalization for
  raw joystick fallback, and less sensitive analog-stick menu navigation. The
  rebuilt `onscripter-new.exe` was copied to
  `D:\Umineko Project\onscripter-new.exe`. No dependency changes were needed.
  Benchmarks, runtime telemetry, and executable boot testing were intentionally
  not run for this input-routing pass.
- The same-date controller follow-up added SDL3 gamepad button and axis events
  to the shared input-event list used by wait and button-monitor actions. This
  keeps normalized gamepad input on the same script-wait path as keyboard and
  raw joystick events, instead of only the default global-event pass. The
  corrected executable was copied to `D:\Umineko Project\onscripter-new.exe`.
  No dependency changes were needed, and executable boot testing was not run.
- The same-date Config input-hints follow-up repacked the active English script
  after renaming the misleading `DualShock` selector to `Gamepad`, changing the
  setting label to `Input Hints`, and making the Controls popup show
  controller-specific bindings when `control_interface=pad`. The packed script
  passed an exact decode round-trip, and the updated `en.file` plus current
  executable were copied to `D:\Umineko Project`. No dependency changes were
  needed, and executable boot testing was not run.
- The same-date controller binding correction added no dependencies. It changes
  only the engine's gamepad scancode mapping plus the packed English Controls
  hint text: L1 now uses a dedicated automode scancode, Triangle/Y remains the
  Message Browser/backlog binding, Options/Start opens the pause menu like
  Square/X, and R2 trigger input is ignored. The UCRT64 rebuild linked
  successfully, the packed script passed an exact decode round-trip, and the
  updated executable plus `en.file` were copied to `D:\Umineko Project`.
- The same-date L1 automode follow-up added no dependencies. It changes only
  the engine event path so the dedicated gamepad automode scancode starts
  automode without storing the same button result used by Cross during
  `textbtnwait`; active text-button waits are armed with the normal automode
  timer/voice-wait behavior. The UCRT64 rebuild linked successfully, and the
  corrected executable was copied to `D:\Umineko Project`.
- The same-date medium whole-codebase performance pass added no dependencies.
  It replaces subtitle decode/display queue spin-sleeps with semaphores,
  streams full-surface `GPU_UpdateImage()` uploads directly to SDL3_GPU,
  routes full-image alpha premultiplication through the existing native
  `multiplyAlpha.frag` shader path, discards stale CPU mirrors when texture
  data is authoritative, and recreates mipmapped textures with a GPU-to-GPU
  base-level copy when possible. The UCRT64 rebuild linked successfully without
  warning output, and the rebuilt `onscripter-new.exe` was copied to
  `D:\Umineko Project\onscripter-new.exe`. Benchmarks, runtime telemetry, and
  executable boot testing were intentionally not run for this source-level
  performance pass.
- The 2026-06-10 Trophies UI polish pass repacked the active English script
  after changing unlocked trophy entries from separate rarity/name/description
  lines to name plus colored rarity parenthetical, then description. The packed
  script passed an exact decode round-trip. `make -j8` reported the binary
  target was already current, and the current `onscripter-new.exe` plus updated
  `en.file` were copied to `D:\Umineko Project`. Per project instruction, the
  executable was not booted.
- The same-date Trophies scrollbar follow-up rebuilt the local Windows/UCRT64
  target after adding shared special-scrollable thumb dragging and correcting
  the capture path to run before the normal `btndown` mouse-down filter used
  by `btnwait2` menus. The rebuilt `onscripter-new.exe` was copied to
  `D:\Umineko Project`; no packed-script rebuild was needed, and per project
  instruction the executable was not booted.
- The same-date Music Box/debug unlock script pass added no dependencies. It
  appends `(Video)` to the Music Box entries that dispatch to opening movies in
  all title-language tables, and adds a `debug_unlock_all` forced config key
  plus a hidden title-screen `C` shortcut that unlocks episodes, read flags,
  character state, omake state, Music Box, CGs, and trophies without touching
  numbered save slots. The packed English script passed an exact decode
  round-trip. `make -j8` reported the binary target was already current, and
  the current `onscripter-new.exe` plus updated `en.file` were copied to
  `D:\Umineko Project`. Per project instruction, the executable was not booted.
- The same-date debug unlock follow-up added no dependencies. It changes the
  hidden unlock path to mark the exact Music Box IDs and Picture Box slots shown
  by those UIs, including Rondo BGM IDs 94 and 1011, display-only Rondo CG slots
  57 through 59, and Chiru-only Music Box entries. The packed English script
  passed an exact decode round-trip. `make -j8` reported the binary target was
  already current, and the current `onscripter-new.exe` plus updated `en.file`
  were copied to `D:\Umineko Project`. Per project instruction, the executable
  was not booted.
- The same-date Music Box selection-display pass added no dependencies. It
  stores each row's localized BGM title in the existing scrollable tree and
  uses the existing BGM-title preset/music-note styling for a centered footer
  now-playing display, clearing it before video entries launch. The packed
  English script passed an exact decode round-trip. `make -j8` reported the
  binary target was already current, and the current `onscripter-new.exe` plus
  updated `en.file` were copied to `D:\Umineko Project`. Per project
  instruction, the executable was not booted.
- The immediate Music Box footer correction added no dependencies. It changes
  the selection-display sprite from story-layer sprite `748` to Music Box-local
  sprite `304` so the footer draws above the menu overlay and scrollable list,
  and it repaints immediately after creating or clearing that sprite. The
  packed English script passed an exact decode round-trip. `make -j8` reported
  the binary target was already current, and the current `onscripter-new.exe`
  plus updated `en.file` were copied to `D:\Umineko Project`. Per project
  instruction, the executable was not booted.
- The same-date Picture Box variant-badge pass added no dependencies. It uses
  existing script text sprites, the existing Picture Box thumbnail grid, and
  existing page-slide animation commands to display small variant counts on
  unlocked thumbnails that open multiple full-image variants. The packed English
  script passed an exact decode round-trip. `make -j8` reported the binary
  target was already current, and the current `onscripter-new.exe` plus updated
  `en.file` were copied to `D:\Umineko Project`. Per project instruction, the
  executable was not booted.
- The immediate Picture Box badge placement correction added no dependencies.
  It only widens the existing badge text sprite, adds explicit border padding
  around the outlined digit sprite, and adjusts the shared badge offset so the
  count sits almost flush with each thumbnail's bottom-right corner. The packed
  English script passed an exact decode round-trip. `make -j8` reported the
  binary target was already current, and the current `onscripter-new.exe` plus
  updated `en.file` were copied to `D:\Umineko Project`. Per project
  instruction, the executable was not booted.
- The same-date Trophies text follow-up added no dependencies. It only changes
  packed-script trophy text aliases: removes the extra manual line breaks from
  the long playtime/click-count descriptions, updates the Platinum description,
  and changes the Platinum rarity color to a cooler PSN-style blue-white. The
  packed English script passed an exact decode round-trip. `make -j8` reported
  the binary target was already current, and the current `onscripter-new.exe`
  plus updated `en.file` were copied to `D:\Umineko Project`. Per project
  instruction, the executable was not booted.
- The same-date title-loading/title-caption follow-up added no dependencies.
  It introduces narrow `cache_wait_img`/`cache_wait_snd` script commands,
  changes the title loading image prewarm from blocking `cache_img` calls to
  queued `async_cache_img` plus a queue wait that pumps UI frames, maps Chiru's
  caption to `Umineko no Naku Koro ni: ~Nocturne of Truth and Illusions~`, and
  color-codes only `<IT'S PERFECTO>!` red in the Platinum trophy description.
  The packed English script passed an exact decode round-trip. The UCRT64
  rebuild linked successfully, and the rebuilt `onscripter-new.exe` plus
  updated `en.file` were copied to `D:\Umineko Project`. Per project
  instruction, the executable was not booted.
- The 2026-06-11 miscellaneous UI/text/title pass added no dependencies. It
  updates the requested playtime and text-advance trophy descriptions and
  changes the pinned Chiru title to the
  `Umineko no Naku Koro ni Chiru: ~Nocturne of Truth and Illusions~` string.
  The attempted Config alignment coordinate changes from that pass were removed
  in an immediate follow-up, restoring the prior Input Hints, Song Subtitles,
  and Textbox Window positions. The packed English script passed an exact
  decode round-trip. The UCRT64 `make -j8` command reported the binary target
  was already current, and the current `onscripter-new.exe` plus updated
  `en.file` were copied to `D:\Umineko Project`. Per project instruction, the
  executable was not booted.
- The 2026-06-12 release-audit cleanup added no dependencies. It updates the
  external IDE helper scripts to copy `onscripter-new`, removes the stale
  `.gitignore` rule for the tracked dependency audit document, and removes two
  byte-identical macOS app-icon PNG duplicates by reusing the existing
  `32x32.png` and `256x256.png` files in the asset catalog. It also requires
  current `onscripter-new` engine outputs for Android APK packaging, removes
  misleading async queue comments, updates current benchmark/telemetry examples
  to the renamed executable, removes an unused sprite-helper method, trims a
  dead sprite-helper parameter, and removes ignored helper-tool object files.
  The broader verification follow-up also updates runtime-visible labels to
  `onscripter-new`, changes Apple bundle display names and Android loading
  text, and fixes Android cross-build portability in libusb-gated joystick
  helpers, explicit standard-library includes, the Droid profiler atomic stop
  path, and SDL2-only touch-threshold declarations. JSON/XML/plist parsing,
  existing PNG decoding, shell syntax, quoted includes, built-in shader
  SPIR-V coverage, packed-script round-trip, Windows release build, Android
  arm64-v8a/x86_64 release build, APK signature verification, APK badging, and
  APK permission checks all passed. The Android APK SHA-256 was
  `F67275ECDC423CC83837713D3CCF0A4567B1D22C76AFB846A0323DB5D3EAFB4A`. The
  final UCRT64 `onscripter-new.exe` SHA-256 was
  `0A1EE6E5E93F6448DF587525D0BC2B597D7224EE83B0306004102DB7A1D58978`; the
  final `en.file` SHA-256 was
  `388C0434DE0CC25CAA1DCA9871517A79911F0EAC6C9A54A1CCDD2A3CB4404DB5`. Both
  were copied to `D:\Umineko Project` and hash-verified. Per project
  instruction, the executable was not booted.

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

## Asset Format Pilot

- The 2026-06-07 PNG size pilot checked both verified PNG recompression and
  lossless WebP/JXL conversion on active Umineko Project assets. PNG
  recompression verified pixel-identical replacements but saved only about
  10 MiB across the initial 151 optimized files, so it is not worth a full
  in-place pass.
- The representative WebP/JXL pilot sampled 30 PNGs from `backgrounds`,
  `graphics`, and `sprites` with five largest plus five random files per
  directory. WebP verified 25 of 30 sampled files with zero pixel mismatches
  and saved 14.51 MiB from the 64.04 MiB it could encode, or 22.66%. The five
  failures were tall ending/credits PNGs over WebP's dimension limit.
- JXL verified all 30 sampled files with zero pixel mismatches and saved
  27.75 MiB from 179.03 MiB, or 15.50%. Applying the directory-level pilot
  rates to the full 5.53 GiB PNG corpus estimates roughly 1.1 GiB saved by a
  universal JXL replacement.
- The measured savings are not large enough to justify adding WebP/JXL runtime
  dependencies, changing asset extensions/loading behavior, or splitting
  oversized assets. The temporary PowerShell pilot scripts were removed, and no
  WebP/JXL asset conversion work is planned for now.

## Remaining Work

1. Run longer in-game visual regression passes across effects, transitions,
   subtitles, video, save/load screens, and resolution/fullscreen changes.
2. Run longer audio regression passes for BGM/SE/voice mixing, looping, fades,
   device changes, and pause/resume behavior under SDL3_mixer, including a
   fade-heavy listening pass after the high-resolution gain and shared
   `dwave`/`ach_prop` fade-priming changes.
3. Verify the GPU-to-GPU mipmap recreation path removes the previous
   `generate_mipmaps`/`ensure_pixels_current` readback pair, then add more
   source attribution for any remaining generic readback callers.
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

## References

- Main compilation guide: `Resources/Docs/Compilation.md`
- libjpeg-turbo releases: https://github.com/libjpeg-turbo/libjpeg-turbo/releases
- Lua release archive: https://www.lua.org/ftp/
- libusb releases: https://github.com/libusb/libusb/releases
- SDL3 GPU API: https://wiki.libsdl.org/SDL3/CategoryGPU
- SDL release archive: https://www.libsdl.org/release/
- SDL_image release archive: https://www.libsdl.org/projects/SDL_image/release/
- SDL_mixer release archive: https://www.libsdl.org/projects/SDL_mixer/release/
