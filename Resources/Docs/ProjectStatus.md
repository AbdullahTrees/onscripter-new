# onscripter-new Project Status

Updated: 2026-06-22

This file consolidates the former compilation guide, dependency audit, and SDL3 performance audit into one maintenance document for the current `onscripter-new` release branch.

---

## Compilation and Build Guide

ONScripter-RU Compilation
=========================

This document does not cover all the compilation steps for every platform, but tries to provide a general idea to give one the right direction on the minimal recommended platform versions as follows:

- macOS 14 (x86_64; arm64 support is the target for new Apple Silicon work)
- iOS 17 (arm64)
- Debian Linux 11 (x86_64)
- Ubuntu Linux 22.04 LTS (x86_64)
- Windows 10 (x86_64)
- Android 14/API 34 (arm64-v8a; x86_64 for emulator/dev builds)

Build stacks use a shared dependency compilation system called **onscrlib** (see `Dependencies` folder for more details). The supported build stacks are as follows:

- Xcode (macOS and iOS support)
- configure & make (everything else)

Project maintenance details now live in this consolidated file.

A generic way to compile the project for the host is as follows:

```
cd /path/to/onscripter
./configure # Add --release-build --strip-binary for release builds
make -j8    # Add DEBUG=1 for debugging
```

The default renderer stack is SDL3_GPU with SDL3_image and SDL3_mixer:

```
./configure
make -j8
```

For packaged game distributions that ship compressed `.file` scripts, use a public release build. Development builds expect the plaintext script layout and can stop at startup with "No compatible game script found" when pointed at release game data.

```
./configure --release-build --strip-binary --std=gnu++23
make -j8
```

On current MSYS2/UCRT64 GCC 16 toolchains, `--std=gnu++23` is recommended for Windows release builds. It keeps the project on the C++23 migration path while retaining GNU dialect compatibility for current libstdc++ headers.

Current local Windows/UCRT64 build commands from PowerShell:

```
C:\msys64\usr\bin\bash.exe -lc 'export MSYSTEM=UCRT64; export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/onscripter-new && ./configure --release-build --strip-binary --std=gnu++23'
C:\msys64\usr\bin\bash.exe -lc 'export MSYSTEM=UCRT64; export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/onscripter-new && make -j8'
```

For an already configured local tree, only the second command is needed. The
current Windows executable is emitted at:

```
D:\onscripter-new\DerivedData\MinGW-x86_64\onscripter-new.exe
```

When the English decoded script changes, rebuild and verify the packed script
from UCRT64:

```
C:\msys64\usr\bin\bash.exe -lc 'export MSYSTEM=UCRT64; export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/onscripter-new && Tools/nscmake/nscmake.exe -o DerivedData/decoded-script/en.file.new DerivedData/decoded-script/en.txt'
C:\msys64\usr\bin\bash.exe -lc 'export MSYSTEM=UCRT64; export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/onscripter-new && Tools/nscdec/nscdec.exe DerivedData/decoded-script/en.file.new DerivedData/decoded-script/en.roundtrip.txt && cmp -s DerivedData/decoded-script/en.txt DerivedData/decoded-script/en.roundtrip.txt'
```

After any successful executable rebuild, copy the executable to the active game
directory. If the packed English script changed, copy it too:

```
Copy-Item -LiteralPath D:\onscripter-new\DerivedData\MinGW-x86_64\onscripter-new.exe -Destination "D:\Umineko Project\onscripter-new.exe" -Force
Copy-Item -LiteralPath D:\onscripter-new\DerivedData\decoded-script\en.file.new -Destination "D:\Umineko Project\en.file" -Force
```

If `sdl3-shadercross.pc` is available in the dependency prefix, SDL3 builds also
define `ONS_USE_SDL3_SHADERCROSS` and link SDL_shadercross. That optional path
lets the SDL3 backend run external SPIR-V/HLSL shaders natively, and it attempts
to translate recognized built-in and simple SDL2_gpu-style fragment GLSL
(`sampler2D`, `varying color`/`texCoord`, scalar/vector uniforms,
`gl_FragColor`) into the native SDL_GPU pipeline before falling back to the
compatibility CPU shader evaluator. Every built-in fragment shader under
`Resources/Shaders` now also has embedded precompiled SPIR-V for the SDL3_GPU
Vulkan path, so Windows SDL3 release builds can run the built-in shader set
natively without runtime shaderc. Runtime telemetry is still the way to identify
external shaders, render-to-self safety fallbacks, or backend-specific native
shader failures.

Runtime GLSL-to-SPIR-V through shaderc is opt-in:

```
./configure --sdl3-runtime-shaderc
```

If `shaderc.pc` is available and `--sdl3-runtime-shaderc` is used, configure defines `ONS_USE_SDL3_SHADERC`. MSYS2's shaderc package links the shared `libshaderc_shared` import library, so release packaging must ship that DLL or use a static shaderc build. Older arbitrary OpenGL GLSL still needs source porting, embedded precompiled bytecode, or a dedicated translator.

The Windows release path links SDL3, SDL3_image, and SDL3_mixer statically, so
a normal package does not need SDL, ANGLE, EGL, GLES, or d3dcompiler renderer
DLLs beside the executable. Runtime validation on Windows reached the Umineko
Project main menu with `--release-build --strip-binary --std=gnu++23`; the log
showed SDL3_GPU initialization, shader compilation, and SDL3_mixer audio
startup without a new Windows application error during the 120 second
validation run.

The SDL3_mixer compatibility layer also exposes high-resolution float volume
wrappers for dynamic fades. BGM and mix-channel property fades keep their
interpolated double volume until the final SDL3_mixer `MIX_SetTrackGain()` call,
while the retired SDL2 fallback path rounded through the legacy SDL2_mixer
volume API.
The 2026-06-02 UCRT64 rebuild after this change linked successfully and the
updated executable was copied to `D:\Umineko Project\onscripter-ru.exe`.
The 2026-06-03 UCRT64 rebuild after full built-in shader SPIR-V coverage also
linked successfully and was copied to the same Umineko Project executable path.
The 2026-06-03 UCRT64 rebuild after native indexed triangle/shader draw
batching also linked successfully and was copied to the same Umineko Project
executable path.
The follow-up 2026-06-03 UCRT64 rebuild added
`--sdl3-benchmark-output <path>` so Windows GUI-subsystem builds can write the
benchmark CSV directly; it linked successfully and was copied to the same
Umineko Project executable path before the benchmark and telemetry comparison
run.
The 2026-06-03 UCRT64 rebuild after the SDL3_mixer channel restart gain fix
also linked successfully and was copied to the same Umineko Project executable
path. This change preserves exact float channel/music gain inside the SDL3
mixer adapter, reapplies gain immediately after `MIX_PlayTrack()`, and clears
stale mix-channel volume properties when explicit `dwave` stop/restart commands
reuse a channel.
The follow-up 2026-06-03 UCRT64 rebuild after the shared `dwave`/`ach_prop`
fade-priming fix also linked successfully and was copied to the same Umineko
Project executable path. This shared fix applied before fallback removal to
both SDL2 and SDL3 builds: when a `dwave*` command starts a channel and the next
script command is a timed `ach_prop` fade for that same numeric channel, the
channel volume is seeded to zero before playback so the scripted fade starts
cleanly instead of from stale engine SFX volume.
The 2026-06-03 SDL3 default cutover cleanup removed the `--sdl2-renderer`
fallback, SDL2_gpu/libepoxy dependency recipes, legacy GL/GLES backend sources,
and old ANGLE/d3dcompiler Windows DLL packaging. Configure now treats SDL3_GPU
as the only renderer backend.
The 2026-06-03 UCRT64 rebuild after this SDL3-only cutover cleanup linked
successfully and the updated executable was copied to
`D:\Umineko Project\onscripter-ru.exe`.
The follow-up 2026-06-03 UCRT64 warning-clean rebuild removed the remaining
GCC diagnostics from joystick ID validation, display-mode initialization,
SDL3-disabled touch gesture handling, PNG load longjmp state, and video
framerate counting. A clean release rebuild emitted no `warning:` lines and the
updated executable was copied again to `D:\Umineko Project\onscripter-ru.exe`.
The same documentation pass refreshed the root `README.md` so the repository is
presented as `onscripter-new`, an Umineko Project ONScripter-RU modernization
branch with SDL3_GPU/SDL3_image/SDL3_mixer as the active build stack.
The 2026-06-03 dependency refresh replaced IJG jpeg 9c with libjpeg-turbo
3.1.4.1, updated Lua to 5.4.8, and updated libusb to 1.0.30. The local UCRT64
release rebuild relinked SDL3_image against the new static JPEG provider,
emitted no `warning:` lines, and was copied to
`D:\Umineko Project\onscripter-ru.exe`.
The 2026-06-03 SDL3 main-menu resource pass rebuilt successfully after adding
SDL3_GPU command-buffer back-pressure, lazy/discardable CPU image mirrors,
direct video-frame row uploads, decoded image-cache budgeting, redundant
surface cleanup, and longer idle waits in the SDL event fetcher. Hardware video
decoding and hardware format conversion now default to enabled unless
`--hwdecoder off` or `--hwconvert off` is supplied. The final UCRT64 executable
was copied to `D:\Umineko Project\onscripter-ru.exe` after the build.
The 2026-06-04 UCRT64 rebuild after the video decode resource pass linked
successfully without warning output and was copied to
`D:\Umineko Project\onscripter-ru.exe`. This pass keeps direct YUV/NV12 video
frames backed by retained FFmpeg `AVFrame` references instead of copying every
plane into new heap buffers, lowers the decoded video frame queue depth while
keeping packet/timing buffering unchanged, skips preallocating RGB staging
surfaces for direct shader-converted videos, releases transient YUV plane and
alpha-mask GPU textures when playback stops, and fixes SWS context teardown to
clear the freed pointer. No benchmark or runtime telemetry pass was run for
this rebuild.
The follow-up 2026-06-04 UCRT64 rebuild after the broader CPU audit linked
successfully without warning output and was copied to
`D:\Umineko Project\onscripter-ru.exe`. This pass stores queued SDL events by
value instead of heap-allocating every event, adds free-list checkout to the
temporary surface/PNG-loader pools, adds a contiguous-copy fast path to SDL3_GPU
texture uploads, caches the last high-level shader-program alias lookup,
reduces idle event-thread wakeups/semaphore traffic, and fixes the video audio
bridge callback write offset. No benchmark or runtime telemetry pass was run
for this rebuild.
The follow-up 2026-06-04 UCRT64 transition-smoothness rebuild restored the SDL
event fetcher idle timeout to 8 ms and restored clear-on-return for temporary
GPU images after choppy transitions were observed in the broad CPU audit build.
The lower-risk allocation, upload, shader lookup, async semaphore, and audio
bridge changes remain in place. The executable linked successfully and was
copied to `D:\Umineko Project\onscripter-ru.exe`. No benchmark or runtime
telemetry pass was run for this rebuild.
The next 2026-06-04 UCRT64 black-transition follow-up restored the temporary
GPU image pool's pre-audit unordered-map scan reuse policy after black
transitions remained choppy. The GPU pool now matches the previous render-target
reuse and clear ordering; the CPU surface/PNG-loader free lists remain in
place. The executable linked successfully and was copied to
`D:\Umineko Project\onscripter-ru.exe`. No benchmark or runtime telemetry pass
was run for this rebuild.
The 2026-06-04 UCRT64 RAM audit rebuild lowered the default decoded image cache
budget from 256 MiB to 64 MiB while keeping `ONS_IMAGE_CACHE_MB` as the runtime
override, changed reusable SDL3_GPU staging-buffer growth from power-of-two
rounding to 256 KiB alignment, and releases the synchronous readback transfer
buffer immediately after the readback copy completes. Playback quality and
shader output are unchanged. The executable linked successfully and was copied
to `D:\Umineko Project\onscripter-ru.exe`. No benchmark or runtime telemetry
pass was run for this rebuild.
The 2026-06-04 UCRT64 audio backend audit rebuild linked successfully after
non-compression audio optimizations in the SDL3_mixer adapter, threaded sound
loader wait path, and FFmpeg video-audio decode path. Decoded chunk loading now
builds the final SDL-owned PCM buffer directly instead of decoding into a
temporary vector and copying again, default one-shot track playback avoids
per-play SDL properties allocation, redundant track-gain writes are skipped,
non-event sound loads use a blocking semaphore wait instead of 1 ms polling,
and already-mixer-format video audio retains FFmpeg frame buffers instead of
copying every decoded frame. The executable was copied to
`D:\Umineko Project\onscripter-ru.exe`. No benchmark or runtime telemetry pass
was run for this rebuild.
The follow-up 2026-06-04 UCRT64 text/sprite rendering audit rebuild linked
successfully after removing several per-frame rendering overheads without
changing playback quality. The pass skips fully transparent glyph draws,
avoids renderable glyph cache lookups for text passes that will not draw,
iterates dialogue pieces directly instead of building temporary pointer deques
during rendering/fade ticks, populates sprite z-levels without an intermediate
ordered sprite set, delays transformed sprite/big-image canvas allocation until
after early exits and clipping, and avoids redundant full-opacity RGBA state
writes for big-image chunks. The executable was copied to
`D:\Umineko Project\onscripter-ru.exe`. No benchmark or runtime telemetry pass
was run for this rebuild.
The 2026-06-04 UCRT64 branding rebuild changed the default Windows executable
target to `onscripter-new.exe`, updated Windows version-resource executable
metadata to match, fixed the window caption to
`Umineko no Naku Koro ni: ~Rondo of the Witch and Reasoning~`, and changed the
user-facing preferred renderer name from `SDL3_GPU` to `Vulkan` while keeping
`SDL3_GPU` as a legacy command-line/config alias. The executable linked
successfully and was copied to `D:\Umineko Project\onscripter-new.exe`.
No benchmark or runtime telemetry pass was run for this rebuild.
The 2026-06-04 UCRT64 menu responsiveness rebuild linked successfully after
adding a Config restart button, async prewarming for Config voice-toggle
character icons, fast Music Box BGM handoff with streaming SDL3_mixer music
loads, and monitor-refresh-driven GUI/event pacing unless `force-fps` is set.
The updated `onscripter-new.exe` was copied to
`D:\Umineko Project\onscripter-new.exe`; the active packed English script was
repacked to `D:\Umineko Project\en.file` after a decode round-trip check, with
the original saved as `D:\Umineko Project\en.file.codex-backup`. A 12 second
startup smoke test kept the process alive until it was intentionally
terminated; no benchmark or longer runtime telemetry pass was run for this
rebuild.
The follow-up 2026-06-04 UCRT64 animation-pacing rebuild linked successfully
after moving GUI/event frame waits from millisecond SDL ticks to the high
resolution SDL performance counter, resetting stale frame debt after long
non-render gaps, preserving dynamic-property interpolation in nanoseconds, and
cross-fading the packed text cursor cells every rendered frame. The updated
`onscripter-new.exe` was copied to `D:\Umineko Project\onscripter-new.exe`.
A 12 second startup smoke test kept the process alive until it was intentionally
terminated; no benchmark or longer runtime telemetry pass was run for this
rebuild.
The 2026-06-05 UCRT64 animation follow-up rebuild linked successfully after
changing packed text cursor smoothing to draw a single full cursor cell with a
refresh-paced alpha phase, avoiding the overbright white-area strobe caused by
cross-fading two semi-transparent cells over the scene. The title Tips submenu
script animation was also changed from repeated immediate `msp`/`waittimer`
steps to refresh-paced `spt` `ypos`/`alpha` properties. The active packed
English script was rebuilt and decode round-trip verified before copy. The
updated `onscripter-new.exe` and `en.file` were copied to `D:\Umineko Project`.
A 12 second startup smoke test kept the process alive until it was intentionally
terminated; no benchmark or longer runtime telemetry pass was run for this
rebuild.
The second 2026-06-05 UCRT64 animation follow-up rebuild linked successfully
after correcting the smoothed cursor to use the bright first cursor cell with
the original fade direction, restoring normal cursor visibility, and converting
the remaining title-menu button open animations for Tea Party, Ura Tea/????,
and Characters from stepped `msp`/`waittimer` sequences to refresh-paced `spt`
properties. The active packed English script was rebuilt and decode round-trip
verified before copy. The updated `onscripter-new.exe` and `en.file` were
copied to `D:\Umineko Project`. A 12 second startup smoke test kept the process
alive until it was intentionally terminated; no benchmark or longer runtime
telemetry pass was run for this rebuild.

The third 2026-06-05 UCRT64 animation follow-up rebuild linked successfully
after adding `spriterangept`/`spriterangeptwait` for grouped sprite-range
position animation and converting the Config page scroll from hundreds of
individual sprite property updates to one refresh-paced grouped range motion.
The active packed English script was rebuilt and decode round-trip verified
before copy. The updated `onscripter-new.exe` and `en.file` were copied to
`D:\Umineko Project`. A 12 second startup smoke test kept the process alive
until it was intentionally terminated; no benchmark or longer runtime telemetry
pass was run for this rebuild.

The follow-up 2026-06-05 Config reset UI script update renamed the Config
button to `Reset Progress`, changed the confirmation prompt and choices, and
centered the reset confirmation layout with a bounded prompt width and padded
choice row. The active packed English script was rebuilt and decode round-trip
verified. `make -j8` reported the binary target was already current; the
current `onscripter-new.exe` and updated `en.file` were copied to
`D:\Umineko Project`. No benchmark, smoke test, or longer runtime telemetry
pass was run for this script-only update.
The immediate follow-up adjusted the reset confirmation choices to match the
Config button color scheme, using white normally and red on hover. The active
packed English script was rebuilt and decode round-trip verified again;
`make -j8` reported the binary target was already current, and the current
`onscripter-new.exe` plus updated `en.file` were copied to
`D:\Umineko Project`. No benchmark, smoke test, or longer runtime telemetry
pass was run for this script-only color update.

The follow-up 2026-06-05 Config polish script update title-cased the visible
Config setting titles, normalized Effect/Voice audio slider vertical placement
to match the BGM row spacing, and added a live textbox-window preview using
the existing `msgwnd` assets. The active packed English script was rebuilt and
decode round-trip verified. `make -j8` reported the binary target was already
current; the current `onscripter-new.exe` and updated `en.file` were copied to
`D:\Umineko Project`. No benchmark, smoke test, or longer runtime telemetry
pass was run for this script-only UI update.
The immediate scroll-regression correction moves the textbox preview sprites
inside the existing Config `190`-`384` animated sprite range and removes the
extra `spriterangept` calls that interrupted page scrolling. The active packed
English script was rebuilt and decode round-trip verified again. `make -j8`
reported the binary target was already current; the current
`onscripter-new.exe` and fixed `en.file` were copied to `D:\Umineko Project`.
No benchmark, smoke test, or longer runtime telemetry pass was run for this
script-only correction.
The follow-up textbox selector layout update removes the TypeN/TypeB/TypeL/
TypeT labels and sample preview text, places the live textbox-window preview in
the selector row, and cycles the selected preview with left/right arrows like
the Song Subtitles option. The active packed English script was rebuilt and
decode round-trip verified again. `make -j8` reported the binary target was
already current; the current `onscripter-new.exe` and updated `en.file` were
copied to `D:\Umineko Project`. No benchmark, smoke test, or longer runtime
telemetry pass was run for this script-only layout update.
The follow-up textbox preview spacing/assets update spreads the selector
arrows outside the preview body, reduces and lowers the row preview for padding
from the Automode Speed slider, and adds preview-only cropped textbox-window
PNGs under `D:\Umineko Project\graphics\system\wnd` so the detached left-side
decorations from the source assets do not appear in Config. The active packed
English script was rebuilt and decode round-trip verified again. `make -j8`
reported the binary target was already current; the current
`onscripter-new.exe` and updated `en.file` were copied to `D:\Umineko Project`.
No benchmark, smoke test, or longer runtime telemetry pass was run for this UI
asset/layout update.
The final textbox preview arrow alignment pass keeps the same cropped preview
assets and adjusts only the packed-script arrow coordinates so both arrows are
vertically centered on the preview and the right-side gap matches the left-side
gap. The active packed English script was rebuilt and decode round-trip
verified again. `make -j8` reported the binary target was already current; the
current `onscripter-new.exe` and updated `en.file` were copied to
`D:\Umineko Project`. No benchmark, smoke test, or longer runtime telemetry
pass was run for this coordinate-only update.
The follow-up empty textbox preview label update adds centered `No Window` text
for the textbox style that intentionally has no window image. The active packed
English script was rebuilt and decode round-trip verified again. `make -j8`
reported the binary target was already current; the current
`onscripter-new.exe` and updated `en.file` were copied to `D:\Umineko Project`.
No benchmark, smoke test, or longer runtime telemetry pass was run for this
script-only label update.
The immediate `No Window` alignment follow-up adjusts only the packed-script
label y coordinate so the text shares the selector arrows' vertical row. The
active packed English script was rebuilt and decode round-trip verified again.
`make -j8` reported the binary target was already current; the current
`onscripter-new.exe` and updated `en.file` were copied to
`D:\Umineko Project`. No benchmark, smoke test, or longer runtime telemetry
pass was run for this coordinate-only update.

The 2026-06-06 pause-menu UI pass renames the in-game pause-menu `Clear`
button artwork to `Hide`, title-cases the session information labels, moves
the episode/chapter artwork into the upper-left menu column above `Load`, and
draws the session information as individually measured right-aligned lines in
the lower-right corner. The active packed English script was rebuilt and decode
round-trip verified. The UCRT64 release executable relinked successfully with
no warning lines in the captured output, and the updated `onscripter-new.exe`,
`en.file`, and active pause-menu artwork were copied to `D:\Umineko Project`.
Per project instruction, the executable was not booted.

The immediate pause-menu follow-up resets all pause-menu button sprites to
their normal cells before re-registering them in the right-click loop, so a
previous hover cell cannot persist when `Hide` returns to the menu. The active
`Hide` button strip was also rebuilt from the original `Clear` artwork backup
with matching normal gray text and a shared cleaned background for normal and
hover cells. The active packed English script was rebuilt and decode
round-trip verified. `make -j8` reported the binary target was already
current; the current `onscripter-new.exe`, updated `en.file`, and corrected
pause-menu artwork were copied to `D:\Umineko Project`. Per project
instruction, the executable was not booted.

The second pause-menu follow-up restores the original active `Clear` button
artwork from the saved backup, keeps the existing lower-right session
information layout but reduces it to a smaller dedicated text preset. The
active packed English script was rebuilt and decode round-trip verified. A
script-side async-loop rebuild attempt was rejected in the next correction
because it prevented pause-menu button input from reaching the existing async
monitor. `make -j8` reported the binary target was already current; the
current `onscripter-new.exe`, updated `en.file`, and restored `Clear` artwork
were copied to `D:\Umineko Project`. Per project instruction, the executable
was not booted.

The emergency pause-menu input correction restores the original right-click
menu async button-wait loop after a script-side no-result rebuild attempt
prevented pause-menu button input from reaching the existing async monitor. The
active packed English script was rebuilt and decode round-trip verified, the
UCRT64 release executable rebuilt successfully, and the updated
`onscripter-new.exe` plus `en.file` were copied to `D:\Umineko Project`. Per
project instruction, the executable was not booted.

The pause-menu hover root-cause follow-up removes the live `_csp` calls from
`*rmenu_draw_time`. Splitting the session information into four right-aligned
sprites had introduced `_csp` calls inside the async button polling loop, and
`cspCommand()` clears normal-sprite button hover bookkeeping even when it is
clearing a non-button text sprite. The session lines now have explicit sprite
aliases and are updated in place with `lsp`/`amsp`, so hover state is not
discarded while the pause menu is waiting for input. The failed engine hover
experiments were reverted, the active packed English script was rebuilt and
decode round-trip verified, `make -j8` rebuilt the executable, and the updated
`onscripter-new.exe` plus `en.file` were copied to `D:\Umineko Project`. Per
project instruction, the executable was not booted.

The session-label follow-up changes the pause-menu `Current Track` line to
display `None` when no BGM track is active instead of leaving the value blank.
The active packed English script was rebuilt and decode round-trip verified.
`make -j8` reported the binary target was already current; the current
`onscripter-new.exe` and updated `en.file` were copied to
`D:\Umineko Project`. Per project instruction, the executable was not booted.

The Tips/Characters detail follow-up recenters the Tips/Grimoire tab artwork
against the visible pixel bounds, moves the Characters Execute/Resurrect action
buttons under the right information panel, and gives those actions persistent
red selected-state labels based on the selected character condition. The active
packed English script was rebuilt and decode round-trip verified. `make -j8`
reported the binary target was already current; the current
`onscripter-new.exe` and updated `en.file` were copied to
`D:\Umineko Project`. Per project instruction, the executable was not booted.

The 2026-06-06 whole-codebase performance audit rebuild removes several
allocation and repeated-lookup costs without changing script timing, draw
order, filtering, decoded samples, or video frame contents. The pass indexes
SAR/NSA archive filenames at load time and reads archived vector buffers
directly, replaces short-lived text-window/dialogue pointer containers with
fixed or reserved storage, stores coalesced touch events inline, uses deque
event queues, avoids repeated cache/StringTree/dynamic-property/GPU pool
lookups, uses fixed SDL3_GPU sampler binding storage for native triangle
batches, reuses `GPU_TriangleBatch` scratch vertices, and retains direct
hardware-converted video frames with `av_frame_clone()`. Three UCRT64 `make -j8`
rebuilds after the engine changes linked successfully. The rebuilt
`onscripter-new.exe` was copied to `D:\Umineko Project`. Per project
instruction, the executable was not booted.

The follow-up 2026-06-06 Config controls script update moves the Config
`Restart` button to the right of `Reset Progress`, places a new `Controls`
button in the previous right-side `Restart` slot, and adds a modal keybind
reference from that button. The active packed English script was rebuilt and
decode round-trip verified. `make -j8` reported the binary target was already
current; the current `onscripter-new.exe` and updated `en.file` were copied to
`D:\Umineko Project`. Per project instruction, the executable was not booted.

A same-day Config controls polish pass renamed the action label to
`Restart Game`, color-coded the keybind groups in the Controls popup with gold
accent text while keeping descriptions white, and centered the popup listing.
The next corrections split the `Controls` header into its own centered text
sprite, removed the dedicated Backlog section from the listing, and moved the
listing body back into the popup foreground sprite range so it renders above
the dim overlay. The fixed-width `1700px` listing box is pinned at `x=110`, and
the width tag now wraps the full `p:11` popup body so every listing line uses
the same centered wrap area. The active English script was repacked and
decode round-trip verified after each pass. `make -j8` reported the binary
target was already current each time; the current `onscripter-new.exe` and
updated `en.file` were copied to `D:\Umineko Project`. Per project instruction,
the executable was not booted.

The 2026-06-07 Apple floor cleanup removed the old macOS/iOS Xcode targets,
the Snow Leopard-era macOS reloader wrapper, ARMv7 helper wrappers, local
static libc++/libc++abi archive links, and obsolete Apple package recipe
branches. The UCRT64 release rebuild linked successfully, and the updated
`onscripter-new.exe` was copied to `D:\Umineko Project\onscripter-new.exe`.
Per project instruction, the executable was not booted.

The follow-up 2026-06-07 dependency removal deleted the legacy libc++ and
libc++abi package recipes plus the Snow Leopard libc++ patch. `make -j8`
reported the binary target was already current; the current
`onscripter-new.exe` was copied to `D:\Umineko Project\onscripter-new.exe`.
Per project instruction, the executable was not booted.

The 2026-06-09 C++23 migration changed the active configure default to C++23,
updated the Windows release command to `--std=gnu++23`, moved Xcode, clang-tidy,
and helper tool Makefiles to C++23, and applied low-risk C++17-C++23 source
cleanups including `std::clamp`, associative-container `contains()`, single-pass
`find()` lookups, and `std::to_underlying()` for libusb request masks. The
UCRT64 release rebuild linked successfully with no warning output after the
libusb enum-mask cleanup, `nscmake` and `nscdec` force-rebuilt with
`-std=c++23`, and the updated `onscripter-new.exe` was copied to
`D:\Umineko Project\onscripter-new.exe`. Per project instruction, the executable
was not booted.

The 2026-06-09 controller support pass moved SDL3 builds onto SDL's normalized
gamepad subsystem for mapped controllers while preserving the raw joystick
fallback. SDL joystick/gamepad add and remove events now open and close devices
at runtime, so a DualShock 4 no longer has to be connected before process
startup. The normalized gamepad map makes Cross confirm, Circle cancel,
Share/Back mute, Options/Start open the tab/message-browser action, leaves
stick clicks unmapped, and raises analog-stick thresholds with release
hysteresis to prevent menu jitter. The Config screen's DualShock/Gamepad
interface selector remains a script/UI prompt setting; controller detection and
input routing are automatic and independent of that option. The UCRT64
`make -j8` rebuild linked successfully, and the rebuilt `onscripter-new.exe`
was copied to `D:\Umineko Project\onscripter-new.exe`. Per project instruction,
the executable was not booted.

The controller follow-up on the same date added SDL3 gamepad button and axis
events to the engine's shared input-event list. Without this, the default event
pass could handle global side effects such as Share/Back mute, but active
dialogue/button wait actions did not see normalized gamepad events and would
not advance text or select buttons. The UCRT64 `make -j8` rebuild linked
successfully, and the corrected executable was copied to
`D:\Umineko Project\onscripter-new.exe`. Per project instruction, the executable
was not booted.

The same-date Config input-hints pass renamed the misleading `DualShock` option
to `Gamepad`, changed the setting label from `Interface` to `Input Hints`, and
made the Controls popup switch between keyboard/mouse and gamepad binding text
based on the saved `control_interface` value. The packed English script
round-tripped exactly through `nscmake`/`nscdec`; `make -j8` reported the
binary target was already current. The current executable and updated `en.file`
were copied to `D:\Umineko Project`. Per project instruction, the executable
was not booted.

The controller binding correction on the same date added a dedicated gamepad
automode scancode for L1, left Triangle/Y on the Message Browser/backlog
binding, mapped Options/Start to the pause-menu binding used by Square/X, and
removed R2 trigger handling from both SDL3 gamepad axes and the raw generic
gamepad map. The gamepad Controls popup text was updated to match. The UCRT64
`make -j8` rebuild linked successfully, the packed English script passed an
exact decode round-trip, and both `onscripter-new.exe` and `en.file` were copied
to `D:\Umineko Project`. Per project instruction, the executable was not
booted.

The L1 automode follow-up on the same date fixed the text-button wait path for
the dedicated gamepad automode scancode. L1 no longer writes button result `0`
directly like Cross; it starts automode and arms the active text-button wait
with the normal automode timer/voice-wait behavior. The UCRT64 `make -j8`
rebuild linked successfully, and the rebuilt `onscripter-new.exe` was copied to
`D:\Umineko Project`. Per project instruction, the executable was not booted.

The 2026-06-09 L1 automode correction removed the dedicated gamepad automode
scancode entirely after the previous two attempts still left L1 advancing text
like Cross without engaging automode. The custom out-of-range scancode never
matched the engine's keyboard `a` automode handler, so the wait-arming side
path ran without the script-visible toggle. L1 (both the SDL3 normalized
gamepad map and the raw joystick fallback `KEYMAP`) now emits
`SDL_SCANCODE_A`, the keyboard automode key, restoring the upstream
ONScripter-RU binding so L1 goes through the exact `keyPressEvent` automode
branch the Umineko Project script's `event_callback`/`jnauto` icon sync was
built against. The `ONS_SCANCODE_AUTOMODE` constant and the special
text-button arming branch were deleted. The UCRT64 `make -j8` rebuild linked
successfully, and the rebuilt `onscripter-new.exe` was copied to
`D:\Umineko Project`. Per project instruction, the executable was not booted.

The same-date L1 automode root-cause fix corrected phantom keyboard events
from suppressed joystick duplicates. SDL3 delivers both a normalized gamepad
event and a raw joystick event for each physical button press; the joystick
duplicate is intentionally ignored for gamepad-managed devices, but
`translateKeyDownEvent`/`translateKeyUpEvent` rewrote the shared event's type
to `SDL_KEYDOWN`/`SDL_KEYUP` before checking whether the translation produced
a valid scancode. Because the event loop dispatches one event once per
registered handler, later handler passes saw the mutated event as a real
keyboard event with scancode `UNKNOWN`, and the "any keypress clears
automode" rule cancelled automode immediately after the gamepad event enabled
it (runtime logging confirmed `change to automode` followed by `automode
cleared by input` from the joystick duplicate on every L1 press, while the
keyboard `a` key worked). The translation functions now translate first and
only mutate the event on a successful mapping, covering joystick button down,
button up, and hat motion paths. A permanent `automode cleared by input` log
line was kept in `checkClearAutomode`. The UCRT64 `make -j8` rebuild linked
successfully, and the rebuilt `onscripter-new.exe` was copied to
`D:\Umineko Project`. Per project instruction, the executable was not booted.

The same-date medium whole-codebase performance pass reduced CPU/RAM work in
renderer and subtitle hot paths without changing script timing math. Subtitle
frame queues now use SDL semaphores instead of 1 ms producer/consumer
spin-sleeps, full SDL surface uploads stream directly into SDL3_GPU transfer
buffers when the destination image is fully covered, full-image alpha
premultiplication falls through to the existing native `multiplyAlpha.frag`
shader path on SDL3, stale CPU image mirrors are discarded when the GPU texture
is authoritative, and mipmap texture recreation copies the base level
GPU-to-GPU instead of downloading and reuploading GPU-rendered screenshot
sources. The UCRT64 `make -j8` rebuild linked successfully without warning
output, and the rebuilt `onscripter-new.exe` was copied to
`D:\Umineko Project`. Per project instruction, the executable was not booted.

The 2026-06-10 Trophies UI script pass consolidated each unlocked trophy entry
from separate rarity/name/description lines into a name plus colored rarity
parenthetical, followed by the description line. The active English script was
repacked and passed an exact decode round-trip. `make -j8` reported the binary
target was already current; the current `onscripter-new.exe` and updated
`en.file` were copied to `D:\Umineko Project`. Per project instruction, the
executable was not booted.

The same-date Trophies scrollbar follow-up added shared special-scrollable
thumb dragging. Clicking the existing up/down arrow buttons and wheel scrolling
continue to use the previous paths, while left-clicking the visible scrollbar
thumb now captures a drag and maps the thumb position back to the scrollable
content offset without returning a script-visible button result. The immediate
correction starts that capture from the raw mouse-down event before normal
`btndown` filtering, since `btnwait2` menus such as Trophies do not enable
button-down reporting by default. The UCRT64 `make -j8` rebuild linked
successfully, and the rebuilt `onscripter-new.exe` was copied to
`D:\Umineko Project`. Per project instruction, the executable was not booted.

The same-date Music Box script pass marks the four video-backed Music Box
entries with a `(Video)` suffix in every title language table: IDs 1012, 1014,
1017, and 1018. It also adds a `debug_unlock_all` forced config key plus a
hidden title-screen `C` shortcut that sets the completed-episode state, chapter
read flags, character unlocks, omake state, Music Box unlocks, CG unlocks, and
trophy unlocks without writing any numbered save slot. The active English
script was repacked and passed an exact decode round-trip. `make -j8` reported
the binary target was already current; the current `onscripter-new.exe` and
updated `en.file` were copied to `D:\Umineko Project`. Per project instruction,
the executable was not booted.

The same-date debug unlock follow-up changed `debug_unlock_all` to unlock the
same Music Box IDs and Picture Box slots that the UIs enumerate directly. This
covers the display-only Rondo CG entries 57 through 59, Rondo BGM IDs 94 and
1011, and the Chiru-only Music Box IDs rather than relying on story loader
side effects. The active English script was repacked and passed an exact decode
round-trip. `make -j8` reported the binary target was already current; the
current `onscripter-new.exe` and updated `en.file` were copied to
`D:\Umineko Project`. Per project instruction, the executable was not booted.

The same-date Music Box selection-display pass stores each row's localized BGM
title in the scrollable tree and shows the active normal BGM selection in a
centered footer sprite on the same line as `Exit`. The display reuses the
existing BGM-title preset, music-note prefix, border padding, and a short
slide-in animation, while video entries clear the footer because they stop
looping Music Box BGM before launching movie playback. The active English
script was repacked and passed an exact decode round-trip. `make -j8` reported
the binary target was already current; the current `onscripter-new.exe` and
updated `en.file` were copied to `D:\Umineko Project`. Per project instruction,
the executable was not booted.

The immediate Music Box footer correction moved the selection-display sprite
from story BGM-title sprite `748` to Music Box-local sprite `304`, which draws
above the menu overlay, scrollable list, and footer controls. The correction
also forces an immediate repaint after creating or clearing the footer sprite.
The active English script was repacked and passed an exact decode round-trip.
`make -j8` reported the binary target was already current; the current
`onscripter-new.exe` and updated `en.file` were copied to
`D:\Umineko Project`. Per project instruction, the executable was not booted.

The same-date Picture Box variant-badge pass adds small outlined count sprites
to unlocked thumbnail slots that open multiple full-image variants. The badge
counts are derived from the actual gallery labels each thumbnail jumps to,
including Chiru slots that reuse Rondo image sequences, and the badge sprites
move with the existing page-slide animation. The active English script was
repacked and passed an exact decode round-trip. `make -j8` reported the binary
target was already current; the current `onscripter-new.exe` and updated
`en.file` were copied to `D:\Umineko Project`. Per project instruction, the
executable was not booted.

The immediate Picture Box badge placement correction widens the badge text
sprite, adds explicit border padding around the outlined digit sprite, and moves
the shared badge anchor almost flush with each thumbnail's bottom-right corner.
The active English script was repacked and passed an exact decode round-trip.
`make -j8` reported the binary target was already current; the current
`onscripter-new.exe` and updated `en.file` were copied to
`D:\Umineko Project`. Per project instruction, the executable was not booted.

The same-date Trophies text follow-up removes the extra manual description-line
breaks from the two long playtime/click-count trophies, updates the Platinum
trophy description to `Collect all other trophies. <IT'S PERFECTO>!`, and
changes the Platinum rarity text to a cooler blue-white that better matches the
PSN platinum trophy palette. The active English script was repacked and passed
an exact decode round-trip. `make -j8` reported the binary target was already
current; the current `onscripter-new.exe` and updated `en.file` were copied to
`D:\Umineko Project`. Per project instruction, the executable was not booted.

The same-date title-loading/title-caption follow-up adds
`cache_wait_img`/`cache_wait_snd` engine commands, converts the title loading
screen's synchronous image prewarm to `async_cache_img`, and waits for that
queue while the loading animation continues to pump UI frames. The Chiru
caption path now maps to the pinned title
`Umineko no Naku Koro ni: ~Nocturne of Truth and Illusions~`, and the Platinum
trophy description color-codes only `<IT'S PERFECTO>!` red. The active English
script was repacked and passed an exact decode round-trip. The UCRT64
`make -j8` rebuild linked successfully; the rebuilt `onscripter-new.exe` and
updated `en.file` were copied to `D:\Umineko Project`. Per project
instruction, the executable was not booted.

The 2026-06-11 miscellaneous UI/text/title pass updates the two long playtime
and text-advance trophy descriptions and changes the Chiru window title to the
`Umineko no Naku Koro ni Chiru: ~Nocturne of Truth and Illusions~` string. The
attempted Config alignment coordinate changes from that pass were removed in an
immediate follow-up, restoring the prior Input Hints, Song Subtitles, and
Textbox Window positions. The active English script was repacked and passed an
exact decode round-trip. The UCRT64 `make -j8` command reported the binary
target was already current; the current `onscripter-new.exe` and updated
`en.file` were copied to `D:\Umineko Project`. Per project instruction, the
executable was not booted.

The 2026-06-12 release-audit cleanup pass updates the external IDE helper
scripts to copy the current `onscripter-new` executable name, removes a stale
`.gitignore` rule for the formerly tracked dependency-audit document,
collapses two byte-identical macOS app-icon PNGs by reusing the existing
`32x32.png` and `256x256.png` asset files for their matching scale slots,
tightens Android APK packaging so it no longer accepts stale `onscripter-ru`
engine outputs, removes misleading async queue comments, updates current
benchmark/telemetry examples to `onscripter-new.exe`, removes unused sprite
helper API surface, and removes ignored helper-tool object files. The broader
verification follow-up also updates runtime-visible message-box/version/error
labels to `onscripter-new`, changes the Apple bundle display names and Android
loading text, and fixes Android cross-build issues found during verification:
libusb-only joystick helpers are now gated with `USE_LIBUSB`, standard library
headers are included where `std::set_new_handler`/`std::terminate` are used,
the Droid profiler atomic stop path and 64-bit fallback symbol formatting are
corrected, and SDL2-only touch action thresholds are no longer compiled for the
SDL3 path.

Verification covered JSON and XML/plist parsing, existing tracked PNG decoding,
shell script syntax, quoted include checks, built-in shader SPIR-V coverage,
exact packed-script decode round-trip, Windows release rebuild, Android
arm64-v8a/x86_64 release rebuild, and APK signature/badging/permission/archive
checks. The Android release APK verified with v2 signing, label
`onscripter-new`, only the `android.permission.VIBRATE` permission, and SHA-256
`F67275ECDC423CC83837713D3CCF0A4567B1D22C76AFB846A0323DB5D3EAFB4A`. The final
UCRT64 executable SHA-256 is
`0A1EE6E5E93F6448DF587525D0BC2B597D7224EE83B0306004102DB7A1D58978`, and the
packed English script SHA-256 is
`388C0434DE0CC25CAA1DCA9871517A79911F0EAC6C9A54A1CCDD2A3CB4404DB5`. Both were
copied to `D:\Umineko Project` and hash-verified. Per project instruction, the
executable was not booted.

The 2026-06-17 Music Box performance pass adds an opt-in `--musicbox-benchmark`
benchmark (with `--musicbox-benchmark-output <path>`) that replicates the
in-game Music Box scrollable draw workload, reports per-stage CSV timings, and
prints a bottleneck analysis identifying the hot paths in
`drawSpecialScrollable()`. The pass then optimizes `drawSpecialScrollable()`
without changing script timing, draw order, or visual output, and without
adding any new dependency. It (1) caches each element's decoded geometry,
text margins, and bg sprite index in `AnimationInfo::ScrollableInfo` and
rebuilds the cache only when the layout generation or `StringTree`
modification version changes, eliminating the per-frame `std::stoi` and
`unordered_map` hash lookups for every visible element; (2) precomputes a
sorted y-end int array so `getScrollableElementsVisibleAt()` lower-bounds over
ints instead of hashing and parsing StringTree branches on every comparison;
(3) splits the draw loop into two passes - all background plates and dividers
first, then all element text - so the SDL3_GPU native blit batch keeps one
source texture per pass instead of flushing a command buffer on every
background/text texture switch (cuts flushes from ~48 to ~2 per frame;
scrollable elements never overlap, so text still composites on top of its own
background identically); and (4) renders each visible element's text once to a
cached GPU texture (the same render-to-texture pattern used for lsp string
sprites) and blits it on subsequent frames, re-rendering only on a tree
mutation or hover-style change, removing the per-frame
`decodeUTF8String` + `layoutSegment` + `layoutLines` + per-glyph blit cost.
`StringTree` gained a `modificationVersion` bumped by `setValue`/`prune`/
`clear` so the caches invalidate on script-facing `tree_set`/`tree_clear`
mutations, and `AnimationInfo::freeScrollableCaches()` releases the cached
text textures on sprite teardown. The synthetic benchmark's full-frame
scrollable draw case improved from ~1468 us/frame to ~57 us/frame (a ~26x
reduction), well within the 144 Hz frame budget of 6944 us. The UCRT64 release
rebuild linked successfully with no warning output, and the updated
`onscripter-new.exe` was copied to `D:\Umineko Project\onscripter-new.exe`
and hash-verified. Per project instruction, the executable was not booted.

The immediate Music Box cache regression follow-up keeps the performance pass
but fixes the two reported regressions. Cached scrollable text textures now
include explicit border/shadow padding, clear their reused render targets
before drawing, and blit back with the inverse padding offset so outlined Music
Box titles are no longer clipped. The cached text key now includes the
effective text/font/style inputs, `scrollable_cfg` invalidates the relevant
geometry/text caches, and the geometry lower-bound cache is limited to the
currently laid-out element range. Cached text textures are no longer copied
into `old_ai` backup sprites, preventing current/backup scrollable states from
sharing and double-freeing the same raw `RenderImage*` on Music Box teardown.
The pass also fixes an older `AnimationInfo::performCopyNonImageFields()` bug
that copied `image_name` from the destination instead of the source object.
The UCRT64 `make -j8` rebuild linked successfully, the
`--musicbox-benchmark --sdl3-benchmark-iterations 20` smoke run completed and
wrote `DerivedData\MinGW-x86_64\musicbox-benchmark-after-cache-fix.txt`, and
the rebuilt `onscripter-new.exe` was copied to `D:\Umineko Project`. A 12
second hidden startup smoke kept the process alive until it was intentionally
terminated. The exact interactive Music Box exit path was not automated, and
no longer runtime telemetry pass was run.

The immediate text/sprite hot-path follow-up keeps the Music Box cache fixes
and removes additional broad per-frame overhead in the engine renderer. Text
piece rendering now walks each glyph buffer once, queues shadow-border,
shadow-glyph, regular-border, and regular-glyph commands into reusable
vectors, and then replays those vectors in the original visual pass order.
This avoids the previous four repeated glyph-buffer scans, skips glyph lookup
work for fully transparent fade frames, and reuses layout-time regular glyph
pointers when the buffer was built with renderable glyphs. Sprite rendering now
stores rebuilt z-order buckets in fixed vectors instead of an
`unordered_map<int, set<...>>`, sorting only buckets with multiple sprites.
Animation timing now scans the tachi/LSP/LSP2 arrays directly instead of
constructing a temporary `std::set<AnimationInfo *>`, and clock advancement
uses a pointer-based helper to avoid per-sprite indexed dispatch overhead while
preserving old-ai recursion, camera motion updates, warp clocks, dirty rects,
and Lua animation timing. No new dependency was added. The UCRT64 `make -j8`
rebuild linked successfully, `git diff --check` reported only the existing
LF/CRLF working-copy warnings, and the 40-iteration Music Box benchmark wrote
`DerivedData\MinGW-x86_64\musicbox-benchmark-after-text-sprite-pass.txt`;
the optimized synthetic full-frame case measured 83.168 us/frame on this
local run. The rebuilt executable was copied to
`D:\Umineko Project\onscripter-new.exe` and hash-verified at SHA-256
`3AC52E68517AC393EEE654CC6773A6B02B6AB8E89D5B31410DCA4D76B10D328B`. A 12
second hidden startup smoke kept the process alive until intentionally
terminated. The interactive Music Box exit path and longer runtime telemetry
were left for manual testing.

The next text/sprite animation continuation further reduces per-frame overhead
outside the Music Box-specific cache. Plain text pieces now carry a
layout-time `needsLayeredRenderPasses` flag; pieces without border or shadow
render through a direct single-pass path instead of filling and replaying the
four command queues. Repeated dialogue piece walks in the text-rendering
monitor, line layout, render preparation, untime path, scrollable text-cache
padding, and string-sprite colour-cell updates now traverse the existing
segment/run/piece containers directly rather than allocating temporary
`std::vector<DialoguePiece *>` lists. Dynamic sprite/spriteset property
animation now erases empty property queues, wait commands re-find queues by
key so erasure is safe, scheduled property application batches sprite and
spriteset flushes to the controller-level flush, cubic slowdown/speedup
equations use direct multiplication instead of `std::pow`, and animated sprite
properties only call `UpdateAnimPosXY()` for x/y motion. Instant properties
still flush immediately. The UCRT64 `make -j8` rebuild linked successfully,
`git diff --check` reported only the existing LF/CRLF working-copy warnings,
the 40-iteration Music Box benchmark wrote
`DerivedData\MinGW-x86_64\musicbox-benchmark-after-dynamic-text-pass.txt`
with `musicbox_full_frame_reordered` at 74.273 us/frame, and the 60-iteration
SDL3 benchmark wrote
`DerivedData\MinGW-x86_64\sdl3-benchmark-after-dynamic-text-pass.csv`. The
rebuilt executable was copied to `D:\Umineko Project\onscripter-new.exe` and
hash-verified at SHA-256
`AF2727810C286AAD08433F4218F55B496781BF7C228D5A784C2BC794C49B75DC`. A 12
second hidden startup smoke kept the process alive until intentionally
terminated. The exact interactive Music Box exit path and longer runtime
telemetry were left for manual testing.

The follow-up shared blit/z-order pass keeps the text and dynamic-property
changes and removes two lower-level costs hit by both text and sprite-heavy
frames. SDL3_GPU native blits now skip the sine/cosine transform path for the
common zero-rotation case used by glyphs, cached text, UI plates, and most
normal sprites; rotated blits still use the previous transform math. Repeated
`GPU_SetRGBA()` calls now no-op when the requested modulation is already set.
Sprite z-order rebuilds now clear only previously populated buckets, record the
currently populated z levels, sort only those buckets, and have
`drawSpritesBetween()` iterate the populated level list instead of scanning all
1000 possible z levels for each scene/HUD/spriteset range. The UCRT64
`make -j8` rebuild linked successfully. The 40-iteration Music Box benchmark
wrote `DerivedData\MinGW-x86_64\musicbox-benchmark-after-blit-zpass.txt` with
`musicbox_full_frame_reordered` at 62.835 us/frame, and the 60-iteration SDL3
benchmark wrote `DerivedData\MinGW-x86_64\sdl3-benchmark-after-blit-zpass.csv`.
The rebuilt executable was copied to
`D:\Umineko Project\onscripter-new.exe` and hash-verified at SHA-256
`3EAF8EB29E94A800846ACE5197DB0E692495B3ED548F386F116D9AF854EA0FF6`. A 12
second hidden startup smoke kept the process alive until intentionally
terminated. The exact interactive Music Box exit path and longer runtime
telemetry were left for manual testing.

The next text/sprite continuation keeps the Music Box crash and clipping fixes
and trims two more hot paths. Dialogue glyph rendering can now defer restoring
glyph image modulation to white until the current text render finishes, avoiding
the previous set/reset color-state pair around every fading glyph while still
restoring touched glyph images before returning from `DialogueController::render`.
Lipsync cell updates now scan the LSP and LSP2 sprite arrays directly instead of
building a temporary `sprites(SPRITE_LSP | SPRITE_LSP2, true)` set for every
character mouth-frame update; old-ai dirty-rect handling is preserved. The
UCRT64 `make -j8` rebuild linked successfully, `git diff --check` reported only
the existing LF/CRLF working-copy warnings, the 300-iteration Music Box benchmark
wrote `DerivedData\MinGW-x86_64\musicbox-benchmark-after-alpha-lips-pass.txt`
with `musicbox_full_frame_reordered` at 59.859 us/frame, and the 300-iteration
SDL3 benchmark wrote
`DerivedData\MinGW-x86_64\sdl3-benchmark-after-alpha-lips-pass.csv`. The rebuilt
executable was copied to `D:\Umineko Project\onscripter-new.exe` and
hash-verified at SHA-256
`C99A1CC7D09ED55FC5E62F98CB3D660561360909ED4A08ABF6D04B189B6016FD`. A 12 second
hidden startup smoke kept the process alive and then exited cleanly after the
scripted close. The exact interactive Music Box exit path and longer runtime
telemetry were left for manual testing.

The follow-up prepared-text/sparse-animation pass keeps the prior fixes and
moves more repeated work out of the frame loop. Persistent dialogue and speaker
name states now prepare per-piece glyph draw commands at layout time, including
shadow, border, regular-glyph, relative-position, and glyph-index data, so live
text rendering no longer rebuilds those command streams or re-walks font-style
markers every draw; one-shot and bounds-only text states skip eager preparation
and use the lazy fallback only if they actually render. Sprite animation
advancement now skips empty LSP/LSP2/tachi slots before entering the recursive
clock helper, `proceedAnimation()` performs the same sparse existence gate
before estimating current/old animation frames, and `estimateNextDuration()`
caches same-branch duration/remaining values instead of querying them twice.
The UCRT64 `make -j8` rebuild linked successfully, `git diff --check` reported
only the existing LF/CRLF working-copy warnings, the 300-iteration Music Box
benchmark wrote
`DerivedData\MinGW-x86_64\musicbox-benchmark-after-prepared-text-sparse-anim-pass-final.txt`
with `musicbox_full_frame_reordered` at 56.991 us/frame, and the 300-iteration
SDL3 benchmark wrote
`DerivedData\MinGW-x86_64\sdl3-benchmark-after-prepared-text-sparse-anim-pass-final.csv`.
The rebuilt executable was copied to `D:\Umineko Project\onscripter-new.exe`
and hash-verified at SHA-256
`DC76C7408426E0F79D6E476BAFE388BCFEE5C5C27103D84042B12FFF2F907CE7`. A 12 second
hidden startup smoke kept the process alive and then exited cleanly after the
scripted close. The exact interactive Music Box exit path and longer runtime
telemetry were left for manual testing.

The next timed-glyph/visible-z pass further trims text and sprite-frame work.
Dialogue segments now keep a prepared list of timed render glyphs populated when
the segment is timed; `advanceDialogueRendering()`, the final-glyph-start
monitor, and segment-completion checks use that list instead of walking every
piece and every glyph in all visible segments. Expired glyphs are compacted out
of the list as the fade finishes. Layered text pieces also compute each glyph's
per-frame fade alpha once and reuse it across shadow, border, and regular glyph
commands. Sprite z-level setup now filters out invisible selected before/after
sprite states before filling z buckets, and repeated scrollable/input/tree/all
sprite command paths now scan LSP/LSP2 arrays directly instead of constructing
temporary ordered `sprites(...)` sets. The UCRT64 `make -j8` rebuild linked
successfully, `git diff --check` reported only the existing LF/CRLF working-copy
warnings, the 300-iteration Music Box benchmark wrote
`DerivedData\MinGW-x86_64\musicbox-benchmark-after-timed-glyph-visible-zpass.txt`
with `musicbox_full_frame_reordered` at 49.519 us/frame, and the 300-iteration
SDL3 benchmark wrote
`DerivedData\MinGW-x86_64\sdl3-benchmark-after-timed-glyph-visible-zpass.csv`.
The rebuilt executable was copied to `D:\Umineko Project\onscripter-new.exe`
and hash-verified at SHA-256
`D0E23A9238099613E12328DD56C321F575D40909AC69C41D85356006B1557E09`. A 12 second
hidden startup smoke kept the process alive and then exited cleanly after the
scripted close. The exact interactive Music Box exit path and longer runtime
telemetry were left for manual testing.

The follow-up cached-alpha/z-range pass removes additional repeated render-time
work. Dialogue glyph fade alpha is now computed when a segment is timed and
updated during `advanceDialogueRendering()`, so `renderPiece()` reads a cached
integer instead of querying `Clock` state for every glyph command; untimed glyphs
reset to full opacity. Sprite z setup now also excludes parent-owned child
sprites and globally hidden LSP/LSP2 groups before filling z buckets, and
`drawSpritesBetween()` lower-bounds into the descending populated z-level list
instead of scanning irrelevant higher levels for each scene/HUD/spriteset range.
The UCRT64 `make -j8` rebuild linked successfully, `git diff --check` reported
only the existing LF/CRLF working-copy warnings, the isolated 300-iteration
Music Box benchmark wrote
`DerivedData\MinGW-x86_64\musicbox-benchmark-after-cached-alpha-zrange-pass-rerun.txt`
with `musicbox_full_frame_reordered` at 51.117 us/frame, and the 300-iteration
SDL3 benchmark wrote
`DerivedData\MinGW-x86_64\sdl3-benchmark-after-cached-alpha-zrange-pass.csv`.
The rebuilt executable was copied to `D:\Umineko Project\onscripter-new.exe`
and hash-verified at SHA-256
`E462F100830E276279F4F7AA58C6266062C3634114ADF515556B0202EFBEC0C6`. A 12 second
hidden startup smoke kept the process alive and then exited cleanly after the
scripted close. The exact interactive Music Box exit path and longer runtime
telemetry were left for manual testing.

The next render-specialization/animation-candidate pass keeps the cached-alpha
and z-range work and removes another layer of repeated branching. Text command
replay now uses separate opaque/static and live-dialogue render paths, so
one-shot/static text rendering does not branch on dialogue state or read glyph
fade fields for every prepared command. The sprite animation clock pass now
builds a compact list of animation candidates while it scans/ticks sprite
clocks, and the following `proceedAnimation()` pass consumes that list instead
of scanning tachi/LSP/LSP2 arrays a second time; it falls back to the full scan
if no candidate list is available. The per-sprite draw path also drops a
canvas-size validation log branch from every `drawToGPUTarget()` call while
keeping the null-target guard. The UCRT64 `make -j8` rebuild linked
successfully, `git diff --check` reported only the existing LF/CRLF working-copy
warnings, the isolated 300-iteration Music Box benchmark wrote
`DerivedData\MinGW-x86_64\musicbox-benchmark-after-render-specialize-anim-candidates-pass-rerun2.txt`
with `musicbox_full_frame_reordered` at 57.239 us/frame, and the 300-iteration
SDL3 benchmark wrote
`DerivedData\MinGW-x86_64\sdl3-benchmark-after-render-specialize-anim-candidates-pass.csv`.
The rebuilt executable was copied to `D:\Umineko Project\onscripter-new.exe`
and hash-verified at SHA-256
`BD04CB6ED52BAC52341997BED4500FE02F62957B14B398F23A9C353681239F27`. A 12 second
hidden startup smoke kept the process alive and then exited cleanly after the
scripted close. The exact interactive Music Box exit path and longer runtime
telemetry were left for manual testing.

The immediate Music Box benchmark regression correction keeps the prior text,
z-range, and animation-candidate work while reducing SDL3_GPU native blit
submission overhead. Compatible native blits targeting the same render target,
pipeline, blend mode, viewport, and scissor now stay in one command buffer even
when the sampled source texture changes; the batch records ordered source
texture/sampler draw groups and rebinds them inside one render pass. The common
zero-rotation blit path also appends quad vertices directly into the native
batch instead of first building and copying a temporary vertex array. Visual draw
order is preserved because draw groups are emitted in queue order. The UCRT64
`make -j8` rebuild linked successfully, the 300-iteration Music Box benchmark
wrote
`DerivedData\MinGW-x86_64\musicbox-benchmark-after-multisource-blit-batch-final.txt`
with `musicbox_full_frame_reordered` at 37.685 us/frame, improving the fresh
pre-fix rerun's 58.797 us/frame and the previous recorded 57.239 us/frame
regression. The same final run measured the formerly alternating single-pass
paths at 57.231 us/frame uncached and 54.395 us/frame cached because source
switches no longer force immediate command-buffer flushes. The 300-iteration
SDL3 renderer benchmark wrote
`DerivedData\MinGW-x86_64\sdl3-benchmark-after-multisource-blit-batch-final.csv`
with `blit_256_batched_submit` at 0.453 us, `triangle_batch_1024_quads_submit`
at 36.229 us, and `readback_full_completed` at 1644.705 us. The rebuilt
executable was copied to `D:\Umineko Project\onscripter-new.exe` and
hash-verified at SHA-256
`DB4B782B5C9323D7F1F50C5A287A9B73AD1E992D70FB2F805081FB1994143E2F`. No
executable boot test or longer runtime telemetry pass was run for this
benchmark-focused fix.

The immediate string-button hover correction fixes a visual regression from the
prepared text-command optimization. Multi-cell text sprites such as the Config
`Controls` button render each cell by reusing one laid-out `TextRenderingState`
and changing `Fontinfo::buttonMultiplyColor` between cells. Prepared glyph draw
commands were still marked valid after that color change, so later hover cells
could reuse the normal-cell glyph pointers and remain white instead of rendering
the red `#FF0000` cell. The cell-rendering loop now invalidates each
`DialoguePiece`'s prepared render commands after applying a new cell multiply
color, forcing the existing lazy preparation path to rebuild glyph commands for
that cell. The UCRT64 `make -j8` rebuild linked successfully, the 300-iteration
Music Box benchmark wrote
`DerivedData\MinGW-x86_64\musicbox-benchmark-after-string-hover-cell-fix.txt`
with `musicbox_full_frame_reordered` at 32.973 us/frame, and the 300-iteration
SDL3 benchmark wrote
`DerivedData\MinGW-x86_64\sdl3-benchmark-after-string-hover-cell-fix.csv` with
`blit_256_batched_submit` at 0.247 us, `triangle_batch_1024_quads_submit` at
39.345 us, and `readback_full_completed` at 1640.318 us. The rebuilt executable
was copied to `D:\Umineko Project\onscripter-new.exe` and hash-verified at
SHA-256 `3466753895003BB9987C1F2C54BC0821D1C8963B363A17A23A13ECF00623F019`.
No script repack, executable boot test, interactive Config hover pass, or
longer runtime telemetry pass was run for this engine-only correction.

The 144 Hz follow-up looked outside text rendering and sprite animation work at
frame pacing and measurement overhead. SDL3_GPU present-mode selection now maps
the renderer's late-swap/default path to `SDL_GPU_PRESENTMODE_MAILBOX` when the
driver supports it, falls back to classic vsync otherwise, and still honors
explicit disabled-vsync benchmark runs with immediate present mode. On SDL3
Windows builds the default swap interval is now late-swap/mailbox rather than
classic vsync; `--force-vsync` restores classic vsync, and the command-line
help text reflects that behavior. The FPS title counter and in-game overlay now
refresh their displayed label at 4 Hz instead of rebuilding text and calling
`SDL_SetWindowTitle()` every frame, and the window controller skips duplicate
title updates. The UCRT64 `make -j8` rebuild linked successfully. A short
`--current-user-appdata --use-logfile` boot from `D:\Umineko Project` confirmed
`SDL3_GPU present mode: mailbox`. The final waited 300-iteration Music Box
rerun wrote
`DerivedData\MinGW-x86_64\musicbox-benchmark-after-fps-present-final-rerun.txt`
with `musicbox_full_frame_reordered` at 33.800 us/frame; the 300-iteration SDL3
benchmark wrote
`DerivedData\MinGW-x86_64\sdl3-benchmark-after-fps-present-final.csv` with
`blit_256_batched_submit` at 0.228 us, `triangle_batch_1024_quads_submit` at
33.625 us, and `readback_full_completed` at 1754.654 us. The rebuilt executable
was copied to `D:\Umineko Project\onscripter-new.exe` and hash-verified at
SHA-256 `3EB3A6EE7F57B939E63E2EEEEDB8AF0C42D5DB4AFA4372315707FF064F0BD776`.
No script repack, long interactive FPS pass, or longer renderer telemetry pass
was run for this pacing/measurement optimization.

SDL3_GPU telemetry can be enabled at runtime with `--sdl3-gpu-telemetry` or
`ONS_SDL3_GPU_TELEMETRY=1`. The renderer logs aggregate command-buffer,
texture-upload, readback, native-draw, CPU-blit, CPU-shader-fallback, and
per-shader native/fallback counters on exit. Texture upload and readback
traffic is also split into per-source buckets such as video frame uploads,
surface copy-out, glyph atlas `simulateRead()`, and CPU fallback paths. The
latest source-tagged Umineko Project runs are summarized in
`Resources/Docs/ProjectStatus.md`; after the embedded glyph/color
SPIR-V and native clipped-clear updates, the measured boot/video path reported
zero CPU shader fallback draws, zero readbacks, and no clear fallback uploads.
The subsequent shutdown-lifetime verification run reported zero CPU fallback,
four 1920x1080 RGBA readbacks attributed to `ensure_pixels_current`/
`generate_mipmaps`, and no Vulkan `VkImage`/`VkImageView` validation leak
output. Full-clear upload fallbacks are split into labels such as
`clear_full_native_fallback_<width>x<height>` and
`clear_full_clipped_upload_<target>_rect_<rect>` to identify target-size and
virtual-resolution causes. A later in-game telemetry point exposed
`alphaOutsideTextures.frag` as a breakup-path CPU fallback source; the current
source now embeds the full built-in fragment shader set as precompiled SPIR-V.
Follow-up telemetry should verify those shaders report native draws and use the
per-shader fallback rows to catch any external or safety fallback path.

The current SDL3_GPU build tracks live `GPU_Image` textures and releases any
remaining SDL_GPU texture objects before `SDL_DestroyGPUDevice()`. The
2026-06-02 shutdown telemetry verification no longer reported the Vulkan
`VkImage`/`VkImageView` shutdown leaks seen in earlier runs. The PNG save path
also keeps libpng `setjmp` handling in a small write helper; the local UCRT64
rebuild linked successfully without the previous GCC `-Wclobbered` longjmp
warnings.

The SDL3 build also includes an opt-in renderer benchmark that exits before game
script initialization, which makes it useful for quick performance comparisons
without unpacked development scripts:

```
./onscripter-new.exe --sdl3-benchmark
./onscripter-new.exe --sdl3-benchmark --sdl3-benchmark-iterations 300 --sdl3-benchmark-width 1280 --sdl3-benchmark-height 720
./onscripter-new.exe --sdl3-benchmark --sdl3-benchmark-output sdl3-benchmark.csv
```

See the SDL3 performance audit section in this file for benchmark cases and current
reference results.

#### macOS and iOS

[Xcode](https://developer.apple.com/xcode/) is a requirement regardless of the compilation method. Use a current Xcode that can target macOS 14 and iOS 17. It is suggested to use [MacPorts](https://www.macports.org), as it is supported by Apple and can provide the necessary tools at easy cost.

1. Install the dependencies required to build onscrlib.
```
sudo port install automake autoconf yasm pkgconfig gmake cmake
```
2. Legacy custom-clang, macOS 10.6, the old macOS reloader wrapper, i386/x86_64h macOS, and armv7/armv7s iOS targets are no longer supported. A custom compiler is no longer required for the supported floor, and the Xcode project now keeps only the supported iOS arm64 and macOS x86_64 application/dependency targets.
3. Run Xcode and select the project of your choice:
    - `onscripter-ru-ios` for native compilation for iOS 17+ arm64
    - `onscripter-ru-osx64` for macOS 14+ x86_64
4. Set custom working directory in `Edit Scheme` -> `Options`.
5. Set build configuration (`Release` or `Debug`) in `Edit Scheme` -> `Options`.
6. Build and debug.


**NOTES**:

- It is recommended to use project-relative path to DerivedData in Xcode preferences, as some build scripts assume it by default.
- You can obviously attempt to use command line compilation. Follow Linux recommendations after installing the dependencies. This is not a supported option and has limitations with Cocoa integration.
- For iOS ipa generation use `Scripts/ipabuild.tool` after compiling the app in Xcode.

#### Cross-compiling for Windows

Cross compiling is the easiest way to get Windows binaries.

1. Install the MinGW-W64 dependencies for x86_64. On macOS this could be done with a MacPorts command:
```
sudo port install x86_64-w64-mingw32-binutils x86_64-w64-mingw32-crt x86_64-w64-mingw32-gcc x86_64-w64-mingw32-headers
```
2. Run the necessary commands:
```
cd /path/to/onscripter
export CC=x86_64-w64-mingw32-gcc
export CXX=x86_64-w64-mingw32-g++
export LD=x86_64-w64-mingw32-ld
export AR=x86_64-w64-mingw32-ar
export RANLIB=x86_64-w64-mingw32-ranlib
export AS=x86_64-w64-mingw32-as
chmod a+x configure
./configure --cross=x86_64-w64-mingw32
make
```

#### Host-compiling Windows

Windows compilation is normally the most difficult one due to Linux build tools ported not ideally to a Microsoft system.

You will need these tools:

* [MSYS2](https://msys2.github.io/) (pick an installer file according to your system architecture)
* [CLion](https://www.jetbrains.com/clion/) or [CodeLite](http://codelite.org/) for a more convenient debugging interface (optional)

1. Install MSYS2 to `C:\msys64` (installing to other locations and using CLion require one to change `MSYS_PATH` in CMakeLists.txt).
2. Update MSYS2 core (use `ucrt64.exe` for native Windows builds):
```
pacman -Syu
```
3. Close MSYS2 at that point and run the following command after reopening it:
```
pacman -Syu
```
4. Repeat the previous action until you are fully updated.
5. Install the required packages via pacman:
```
pacman -S --needed base-devel git mercurial subversion unzip zip p7zip yasm nasm pkgconf autoconf automake libtool make patch gettext-devel mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-meson mingw-w64-ucrt-x86_64-clang mingw-w64-ucrt-x86_64-yasm mingw-w64-ucrt-x86_64-nasm mingw-w64-ucrt-x86_64-openssl mingw-w64-ucrt-x86_64-curl
```
6. Optionally install these packages:
```
pacman -S mingw-w64-ucrt-x86_64-gdb
```
7. Proceed using the generic method of compilation at the beginning of these instructions. Provide `--prefer-clang` configure argument if using Clang.

**NOTES**:

- GDB may find no source in your executable, `make DEBUG=1` is needed to build a debug binary.
- If you need to build a shared SDL2 library, after you change `--disable-shared` to `--disable-static` you may get an error on compilation step with `SDL_window_main.o` not found. To fix that you are in need to go to SDL2 sources and copy the contents of build/.libs to build (perhaps one more time after next step). Then manually run `make` and `make install`. To mark the package as built run `touch onscrlib/onscrlib/.pkgs/SDL2`.
- Latest gdb versions from MSYS2 distribution do not always work properly in Codelite. A slightly older mingw build may be more stable (try [gdb2014-05-23.zip](https://sourceforge.net/projects/gdbmingw/files/)).
- You may run into issues if you forget to start MSYS2 via `ucrt64.exe`.
- You must remember that MSYS2 uses linux-style slashes for paths. This means a path `C:\Directory\AnotherDir` should be written as `/c/Directory/AnotherDir` in MSYS2.
- First compilation must be performed outside of CLion due to several incompatibilities.
- Using `make -j4` or similar is prohibited for the first compilation and is not recommended when building with gcc due to MinGW issues.

**Using CLion**:

As an alternative to Codelite you may use CLion IDE created by JetBrains. Copy the onscripter/.idea folder inside your onscripter directory with configured target and open the project from CLion IDE. Most of the actions can be found by pressing Ctrl+Shift+A combination and typing them. A short problem list includes:

- Uneasy navigation (Use favourites window with project viewer in file structure mode)
- Slow step-by-step debugging (decrease Value tooltip display in Debug settings)
- Missing class variables when debugging (disable Hide out-of-scope variables option)
- Annoying typo finds (disable spelling correction in settings)
- No line numbers ("Editor" â†’ "Appearance" â†’ "Enable line numbers")
- Spaces instead of TABs (enable Use TAB character and disable Detect and use existing file indents for editing)

#### Android

**Prerequisities**:

- everything necessary to build a hosted engine
- wget or curl command line tool
- JDK 17+ for APK packaging
- Android SDK platform 36, Platform Tools, and NDK r29
- Android SDK Build Tools 36.1.0 for local APK inspection/signature verification

**Basic compilation guide**:

This guide is useful for development when targeting a single device with a single architecture:

1. Run configure for your target architecture:
```
./configure --droid-build --droid-arch=arm64
```
The supported target architecture is `arm64`; `x86_64` is available for emulator and development builds. The configure script will use NDK r29 from the Android SDK if present, or set up wrapper toolchains as needed. All the normal configure options from the beginning of the document apply.
2. Make the engine:
```
make -j8
```
3. Create the apk and grab it from the Droid-package subfolder in the build directory:
```
make apk
```

Android packaging is Gradle/Android Gradle Plugin based. The checked-in Gradle
wrapper uses Gradle 9.4.1 and the package project uses Android Gradle Plugin
9.2.0, `compileSdk 36`, `minSdk 34`, `targetSdk 36`, AAPT2, D8, zipalign, and
apksigner. The package id remains `org.umineko_project.onscripter_ru` to
preserve existing installs and save locations, while the visible app label is
`onscripter-new`.

Android no longer requests broad storage permissions. `ONSActivity` maps the
engine's storage environment to app-scoped external storage under:

```
Android/data/org.umineko_project.onscripter_ru/files/ONScripter-RU
```

**Multiple architecture compilation guide**:

To compile for multiple architectures (i.e. create a FAT apk file) for deployment you could either use `./Scripts/quickdroid.tool` tool or run the following commands manually:
```
./configure --droid-build --droid-arch=arm64
make
./configure --droid-build --droid-arch=x86_64
make
make apkall
```

The final multi-ABI release APK is written to:

```
DerivedData/Droid-package/onscripter-new.apk
```

`./Scripts/quickdroid.tool` accepts the following arguments:
- `--normal` â€” normal developer build (default)
- `--release` â€” stripped release build
- `--debug` â€” debug build

**Debugging the binaries**:

It is recommended to debug using IDA Pro.

1. Setting Java debugger in order to properly start the application. It is worth checking the [official documentation](https://www.hex-rays.com/products/ida/support/tutorials/debugging_dalvik.pdf) first.

    1. Open classes.dex in (32-bit) IDA Pro by dragging onscripter-new.apk into its main window
    2. Put a breakpoint on `_def_Activity__init_@V`
    3. Go to `Debugger` â†’ `Debugger options` â†’ Set specific options and fill adb path
    4. Launch the debugger and specify source path mapping (`.` â†’ `path/to/onscripter/sources`)

2. Setting hardware debugger in order to debug the binary.

    1. Open `libmain.so` in IDA Pro by dragging `onscripter-new.apk` into its main window
    2. Set debugger to `Remote Linux Debugger`
    3. Upload a correct android debugger server to the device (e.g. to `/data/debug/`):
        - `android_server64` â€” for arm64
        - `android_x86_64_server` â€” for x86_64

        You may use the following command:
        ```
        adb push android_server64 /data/debug/
        ```

    4. Set debugger executable permissions to 0777 and run the debugger (use adb shell).
    5. Set `Debugger` â†’ `Process` options parameters:
        - Application and Input file to your device libmain.so path, e.g.:
            ```
            /data/app/~~*/org.umineko_project.onscripter_ru-*/lib/arm64/libmain.so
            ```
        - Hostname to your device IP address, e.g.:
            ```
            192.168.1.111
            ```
        - Directory to your src directory, e.g.:
            ```
            path/to/onscripter/sources
            ```
    6. Ignore any warnings.
    7. Add a breakpoint to `SDL_main`

3. Using the debugger.

    1. Start the process in IDA Java instance
    2. Attach to the process in IDA Native instance
    3. Detach from the process in IDA Java instance (or just ignore it)
    4. Enjoy

**NOTES**:

- Java 17+, Android platform 36, and NDK r29 are required for supported APK packaging.
- Only arm64-v8a and x86_64 binaries are compiled.
- Building standalone onscrlib package may fail on Windows due to `%PATH%`/`$PATH` design
- Source level debugging may not always be available
- The logs are generated with ONScripter-RU and SDL tags:
```
adb logcat | grep -E '(ONScripter-RU|SDL)'
```

#### Building Android APKs

Android Java sources are compiled by Gradle during APK packaging.

1. Download a current [Java SE Development Kit](https://www.oracle.com/java/technologies/downloads/) or OpenJDK distribution for your platform.
2. Download [Android command line tools](https://developer.android.com/studio/index.html#downloads) for your platform (avoid Android Studio itself).
3. Extract the downloaded tools some folder e.g. `$HOME/droid/tools`
4. Install the following packages:
    - `platform-tools` (Android SDK Platform-tools)
    - `build-tools;36.1.0` (Android SDK Build-tools)
    - `platforms;android-36` (Android 16/API 36 SDK Platform)
    - `ndk;29.0.14206865` (Android NDK r29)

    On Windows run android.bat and manually uncheck everything else.
    On Other platforms you could run the following command:
    ```
    ./bin/sdkmanager platform-tools 'platforms;android-36' 'build-tools;36.1.0' 'ndk;29.0.14206865'
    ```

5. Build an APK from existing Android engine outputs by running one of:
    ```
    ./Scripts/apkbuild.tool DerivedData/Droid-aarch64 --release
    ./Scripts/apkbuild.tool DerivedData/Droid-x86_64 --debug
    ./Scripts/apkbuild.tool DerivedData --release
    ```

    The following arguments are supported:

    - `--release` - builds and signs the release variant
    - `--debug` - builds the debug variant

    `--jsign` and `--no-recompile` are obsolete and intentionally rejected.

    The following environment variables are supported:

    - `JAVA_PATH` - path to the JDK `bin` directory
    - `JAVA_HOME` - JDK root used when `JAVA_PATH` is not set
    - `DROID_SDK_ROOT`, `ANDROID_SDK_ROOT`, or `ANDROID_HOME` - Android SDK root
    - `DROID_TOOLS` - path to Android build-tools, used to infer the SDK root
    - `DROID_PLATFORM` - path to `android.jar`, used to infer the SDK root
    - `ONS_ANDROID_KEYSTORE` - release signing keystore path
    - `ONS_ANDROID_KEYSTORE_PASSWORD` - release signing keystore password
    - `ONS_ANDROID_KEY_ALIAS` - release signing key alias
    - `ONS_ANDROID_KEY_PASSWORD` - release signing key password

#### Linux Ubuntu 22.04 LTS/Debian 11/SteamOS

1. You will need a number of packages:
```
apt-get install build-essential cmake subversion mercurial git libreadline-dev autoconf yasm libasound-dev libgl1-mesa-dev libegl1-mesa-dev libxrandr-dev pkg-config unzip
```
    1. You may not need mesa GL packages if you use NVIDIA proprietary drivers.
2. Don't forget to set an executive bit for shell files, otherwise it may fail:
```
chmod +x configure Scripts/* Dependencies/build.sh
```
3. Proceed using the generic method of compilation at the beginning of these instructions.

_Unlike macOS (manual architecture specification) and Windows (untested 64-bit binary), the executable architecture on Linux depends on the default compiler architecture. On 32-bit systems 32-bit binaries are normally produced and on 64-bit systems â€” 64-bit binaries._

#### arm64 Debian

Same as above but you will need to add `--prefer-clang` to the configure script when you proceed using the generic method of compilation at the beginning of these instructions. Not before installing the required clang packages however:

```
apt-get install clang libclang-dev
```

#### Linux OpenSUSE

1. You will need a number of packages (similarly to Debian). Install them with YaST or whatever you like:
```
cmake, autoconf, automake, yasm, mercurial, subversion, git, libtool, gcc-c++, patch, readline-devel, Mesa-libGL-devel, freeglut-devel, glibc-devel-static, alsa-devel
```
2. Don't forget to set an executive bit for shell files, otherwise it may fail:
```
chmod +x configure Scripts/* Dependencies/build.sh
```
3. Proceed using the generic method of compilation at the beginning of these instructions.

---

## Dependency Audit and Modernization Plan

# Dependency Audit and Modernization Plan

Date: 2026-05-31
Updated: 2026-06-17

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
- The 2026-06-17 Music Box cache regression follow-up adds no third-party
  dependencies. It only hardens the existing special-scrollable geometry/text
  caches, cache invalidation, and `AnimationInfo` copy semantics, while keeping
  the SDL3_GPU benchmark and renderer stack unchanged.
- The 2026-06-17 Music Box text/sprite optimization follow-ups add no
  third-party dependencies. They only change existing dialogue rendering,
  dynamic-property scheduling, sprite animation/z-order traversal, SDL3_GPU
  state guards, and lipsync sprite scans.

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
- The 2026-06-17 Music Box cache regression follow-up added no dependencies.
  It only hardens existing special-scrollable geometry/text caches, cache
  invalidation, and `AnimationInfo` copy semantics while keeping the SDL3_GPU
  benchmark and renderer stack unchanged.
- The 2026-06-17 Music Box text/sprite optimization follow-ups added no
  dependencies. They only change existing dialogue rendering, dynamic-property
  scheduling, sprite animation/z-order traversal, SDL3_GPU state guards, and
  lipsync sprite scans.

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

- Main maintenance guide: `Resources/Docs/ProjectStatus.md`
- libjpeg-turbo releases: https://github.com/libjpeg-turbo/libjpeg-turbo/releases
- Lua release archive: https://www.lua.org/ftp/
- libusb releases: https://github.com/libusb/libusb/releases
- SDL3 GPU API: https://wiki.libsdl.org/SDL3/CategoryGPU
- SDL release archive: https://www.libsdl.org/release/
- SDL_image release archive: https://www.libsdl.org/projects/SDL_image/release/
- SDL_mixer release archive: https://www.libsdl.org/projects/SDL_mixer/release/

---

## SDL3 Performance Audit

# SDL3 Performance Audit

Date: 2026-06-02
Updated: 2026-06-17

This audit covers the SDL3 default renderer path, with emphasis on
`Engine/Graphics/SDL3GPUCompat.cpp` because that layer currently adapts the
engine's former SDL2_gpu-shaped renderer API to SDL3_GPU.

## Benchmark

The SDL3 build now has an opt-in benchmark mode that exits before game script
initialization:

```sh
./onscripter-new.exe --sdl3-benchmark
```

Optional arguments:

```sh
./onscripter-new.exe --sdl3-benchmark \
  --sdl3-benchmark-iterations 300 \
  --sdl3-benchmark-width 1280 \
  --sdl3-benchmark-height 720 \
  --sdl3-benchmark-output sdl3-benchmark.csv
```

The benchmark reports CSV columns:

```text
case,iterations,total_ms,avg_us
```

Reference result on the local Windows UCRT64 SDL3 public-release build after
native indexed triangle/shader draw batching:

| Case | Iterations | Total ms | Average us |
| --- | ---: | ---: | ---: |
| `texture_upload_full_submit` | 300 | 137.920 | 459.734 |
| `clear_full_submit` | 300 | 11.363 | 37.875 |
| `clear_full_copy_surface_completed` | 300 | 304.707 | 1015.689 |
| `clear_rect_256_submit` | 300 | 124.835 | 416.116 |
| `blit_256_immediate_flush` | 300 | 50.417 | 168.058 |
| `blit_256_batched_submit` | 300 | 0.065 | 0.216 |
| `triangle_batch_1024_quads_submit` | 300 | 17.994 | 59.980 |
| `readback_full_completed` | 300 | 364.422 | 1214.739 |

The previous pre-native-triangle-batching reference for
`triangle_batch_1024_quads_submit` was 52.388 ms total / 174.628 us average.
The current 17.994 ms total / 59.980 us average is a 65.7% reduction for the
synthetic compatible triangle batch case. The CSV was captured with
`--sdl3-benchmark-output` because the Windows executable uses the GUI
subsystem.

The submit cases measure CPU-side compatibility-layer cost and command
submission cost. `blit_256_immediate_flush` forces the old per-blit submission
shape, while `blit_256_batched_submit` queues the same workload and flushes
once. `clear_full_copy_surface_completed` copies out a surface after each lazy
full clear without forcing the persistent CPU mirror to become current.
`readback_full_completed` intentionally waits for GPU work and downloads the
full render target on each iteration, so it should be treated as a
synchronization cost rather than a normal per-frame cost.

## Fixes Applied

### Release-Audit Cleanup

The 2026-06-12 release-audit cleanup made no renderer, mixer, or timing code
changes and adds no new benchmark data. It fixes stale external IDE helper
script output names after the `onscripter-new` rename, removes the stale
`.gitignore` entry for the tracked dependency audit document, and removes two
byte-identical macOS app-icon PNG duplicates by reusing the surviving same-size
assets in the asset catalog. It also tightens Android APK packaging to require
current `onscripter-new` engine outputs, removes stale async queue comments,
updates current benchmark/telemetry examples to `onscripter-new.exe`, and
removes unused sprite-helper API surface plus ignored helper-tool object files.
The verification follow-up also fixes Android cross-build portability issues in
non-renderer code and verifies the Android release APK. The active English
script was repacked and passed an exact decode round-trip; the Windows release
build relinked successfully, the Android arm64-v8a/x86_64 release build and APK
packaging passed, and the rebuilt `onscripter-new.exe` plus updated `en.file`
were copied to `D:\Umineko Project` with matching hashes. Per project
instruction, no executable boot test, benchmark, or runtime telemetry pass was
run.

### Reusable Upload Buffers

Before this audit, every native blit and triangle batch created and released
fresh SDL_GPU vertex, index, and transfer buffers. Texture uploads also created
and released a new transfer buffer for every upload. Those allocations were in
hot paths and happened at SDL2_gpu draw-call frequency.

The SDL3 backend now keeps reusable, grow-only transfer/upload buffers for:

- texture uploads
- vertex data uploads
- index data uploads

Buffers are cycled through SDL3_GPU's `cycle` parameters, so reuse avoids stale
GPU dependencies while removing repeated allocation and release overhead.

### Full Clear Fast Path

Full-target clears previously filled CPU backing pixels and uploaded the whole
texture. This was especially expensive for temporary render targets, fullscreen
mode changes, and target pool reuse.

Full clears now:

- keep the CPU backing pixels consistent for later readbacks
- clear the GPU texture with a render-pass clear
- avoid a full texture upload

Partial rectangle clears still update the CPU backing store and upload only the
affected region.

### Native Blit Batching

Fixed-pipeline SDL3 native blits now queue consecutive compatible draws instead
of acquiring, encoding, and submitting a command buffer per blit. The batch key
captures target, texture, pipeline, sampler, viewport, scissor, and blend state.
The queue flushes before CPU-visible operations, texture updates, clears,
triangle batches, shader state changes, presentation, and image destruction.

The benchmark's same-state 256x256 blit case improved from about 361.252 us per
forced immediate flush to about 0.851 us per queued blit plus one batch flush.

### Native Indexed Triangle and Shader Draw Batching

Compatible fixed-pipeline `GPU_TriangleBatch` draws and native shader-program
indexed draws now queue into a native triangle batch instead of submitting one
SDL_GPU command buffer per call. The batch key captures target, render target
texture, pipeline, samplers, viewport, scissor, blend state, shader kind, and a
snapshot of the native fragment uniform registers.

Existing public flush boundaries now flush both native queues. Switching from
queued triangles to queued blits, or from queued blits to queued triangles,
flushes the previous queue first so draw order is preserved. CPU shader
fallbacks still flush pending native work before touching CPU backing storage.

Status: benchmark-verified on the local Windows UCRT64 SDL3 build and copied to
`D:\Umineko Project\onscripter-ru.exe`. The benchmark's compatible
`triangle_batch_1024_quads_submit` case improved from 174.628 us to 59.980 us
average per iteration after batching. A user-controlled startup/video telemetry
run after this change recorded zero readbacks and zero CPU blit/shader fallback
pixels; effect-heavy telemetry is still useful for validating command-buffer
behavior in less synthetic scenes.

### Lazy CPU Mirrors and Reused Readback Buffers

Full clears now mark CPU backing storage as a lazy solid color instead of
writing every pixel immediately. CPU fallback materializes that solid color only
when it actually needs persistent pixel data. Screenshot/save-style surface
copy-out can fill the destination surface directly from the solid color or
download GPU data directly into the surface, avoiding unnecessary persistent
mirror synchronization. GPU readback also reuses a grow-only download transfer
buffer instead of allocating and releasing a transfer buffer for each completed
readback.

This moves the common full-clear submit case from about 182.538 us to about
30.391 us. Full-clear surface copy-out is still measurable, about 883.049 us in
the benchmark because it includes a full destination surface fill/copy.

### Native Shadercross for Recognized Built-In Shaders

Recognized built-in fragment shaders are no longer forced into the SDL3 CPU
compatibility evaluator when the build has SDL_shadercross. The SDL3 backend now
first attempts to translate legacy SDL2_gpu-style GLSL to HLSL and compile it
through SDL_shadercross. The old CPU evaluator remains the fallback for shaders
that cannot be translated or for native draw cases that cannot execute safely
such as render-to-self or incompatible texture formats.

The legacy translator now handles comma-separated uniform declarations, GLSL
texture-coordinate swizzles such as `.st`/`.s`/`.t`, and integer-backed boolean
uniforms. This specifically allows common transition shaders such as
`blendByMask.frag` to use native SDL_GPU fragment programs in shadercross builds
instead of always running per-pixel CPU loops.

### Embedded Native Transition Shader

The common transition shader, `blendByMask.frag`, now has embedded precompiled
SPIR-V for the SDL3_GPU Vulkan path. The backend tries this bytecode before
runtime shadercross translation, so normal Windows SDL3 release builds can run
crossfade and mask transitions as native SDL_GPU fragment programs without
linking shaderc or shipping `libshaderc_shared.dll`.

### Embedded Native Blur and Video Conversion Shaders

The boot-logo blur path uses `blurH.frag` and `blurV.frag`, and video playback
uses `colourConversion.frag` for NV12/YUV420P plane conversion. These shaders
now have embedded precompiled SPIR-V and are selected before shadercross. SDL3
native shader draws also accept R8 and R8G8 sampler textures for video planes,
while render targets remain restricted to RGBA-compatible textures.

### Embedded Native Glyph and Color-Modification Shaders

The measured text/glyph CPU fallback shaders, `glyphGradient.frag` and
`colorModification.frag`, now follow the same embedded precompiled SPIR-V path
as the transition, blur, and video conversion shaders. The backend selects this
bytecode before shadercross and registers the legacy uniform names against the
same 16-byte compatibility register layout used by the CPU evaluator.

`colorModification.frag` is ported as the full shader, not just the observed
glyph tinting branch, so sepia, blur, greyscale, negative, darken, and color
replacement modes remain covered. `glyphGradient.frag` preserves the source
shader's gradient math and premultiplied-alpha output.

### Embedded Native Alpha-Outside-Texture Shader

The breakup triangle path can use `alphaOutsideTextures.frag` for source images
smaller than the full script resolution. The shader samples the source texture
but returns transparent black when interpolated texture coordinates are outside
the normalized 0..1 texture box. That behavior is necessary because SDL3_GPU
samplers clamp to edge, while the breakup triangles can intentionally generate
out-of-range coordinates.

`alphaOutsideTextures.frag` now has embedded precompiled SPIR-V with one sampler
and no fragment uniform buffer. The Windows public release build links this
bytecode directly, avoiding runtime shaderc. Runtime verification is pending the
next telemetry pass at the breakup-heavy point that exposed this fallback.

### Complete Built-In Fragment Shader SPIR-V Coverage

The remaining built-in fragment shaders now have embedded precompiled SPIR-V
for the SDL3_GPU Vulkan path: `breakup.frag`, `cropByMask.frag`,
`effectTrvswave.frag`, `effectWarp.frag`, `effectWhirl.frag`,
`glassSmash.frag`, `mergeAlpha.frag`, `multiplyAlpha.frag`, `pixelate.frag`,
`renderSubtitles.frag`, and `textFade.frag`. Combined with the previously
embedded transition, blur, video conversion, glyph/color-modification, and
alpha-outside-texture shaders, the static `Resources/Shaders/*.frag` inventory
now has no built-in fragment shader without native Vulkan bytecode.

The native shader path also gained sampler-to-image-unit mapping. This lets
`renderSubtitles.frag` bind the subtitle atlas as its only native sampler while
rendering into the current subtitle frame; the render-to-self guard still
rejects native execution when a shader would actually sample its render target.

The 2026-06-03 UCRT64 build linked successfully after this change, and the
updated executable was copied to `D:\Umineko Project\onscripter-ru.exe`.
Runtime verification is still needed across representative scenes because
telemetry is the authoritative way to catch external shaders, render-to-self
safety fallbacks, unsupported texture formats, or backend-specific native
shader creation failures.

### SDL3-Only Build and Windows Packaging Cutover

The fallback-release window is closed. Configure now builds the SDL3_GPU path
unconditionally, rejects `--sdl2-renderer`, and no longer wires SDL2_gpu or
libepoxy into `onscrlib`. The legacy GL2/GLES2/GLES3 backend source files were
removed, Xcode references to those files and to SDL2_gpu/libepoxy archives were
scrubbed, and Windows packaging no longer ships ANGLE, EGL/GLES, or
d3dcompiler renderer DLL payloads.

Status: build-verified on the 2026-06-03 local Windows UCRT64 SDL3 public
release build and copied to `D:\Umineko Project\onscripter-ru.exe`. Runtime
telemetry is still useful for representative shader/effect validation, but this
cleanup changes the dependency graph and packaging surface rather than renderer
runtime semantics.

### Android SDL3 Packaging Cutover

Android now uses the SDL3 3.4.10 Java activity/support sources with a
Gradle/Android Gradle Plugin package project instead of the legacy checked-in
`classes.dex`, `resources.arsc`, and custom `aapt`/`d8` script flow. The
manifest targets SDK 36, the package keeps min SDK 34, native libraries are
packaged for `arm64-v8a` and `x86_64`, and `ONSActivity` maps engine storage to
the app-scoped external files directory.

Status: local Android release builds completed for arm64-v8a and x86_64, and a
multi-ABI APK verified with apksigner/aapt. This changes the Android packaging,
storage, and SDL Java integration surface; no renderer benchmark was run because
the SDL3_GPU hot path was not changed.

### SDL3_mixer High-Resolution Dynamic Fades

Dynamic BGM fades (`abgm_prop`) and mix-channel fades (`ach_prop`) interpolate
their property values in double precision, but the SDL3_mixer compatibility
surface previously exposed only SDL2-style integer volume calls. That quantized
each fade update to SDL_mixer's 0..128 volume scale before reaching SDL3_mixer's
float track-gain control, which can make fade-in and fade-out changes sound
stepped.

The compatibility boundary now exposes `Mix_VolumeFloat()` and
`Mix_VolumeMusicFloat()`. SDL3 builds send the exact float gain to
`MIX_SetTrackGain()`, while SDL2 fallback builds round through the legacy
integer SDL2_mixer API. The engine-side `setVolume()`, `setMusicVolume()`, and
`setCurMusicVolume()` helpers now accept double levels, and dynamic channel
fades validate fractional volume without truncating through `validVolume()`.
Static script volume commands still use the existing integer 0..100 validation.

The SDL3_mixer adapter also keeps exact float channel/music volume in its own
track state and reapplies gain immediately after `MIX_PlayTrack()`. Explicit
`dwave` stop/restart commands clear any queued mix-channel volume property for
the reused channel, preventing an old `ach_prop` fade from mutating the next
sample. This targets the project-logo/Witch Hunt splash path where
`umilse_016.ogg` starts on channel 16 and is later faded to zero.

Follow-up status: the splash dip also reproduced in the SDL2 fallback build, so
the remaining issue was shared command sequencing rather than SDL3_mixer gain
handling. The decoded Umineko script starts channel 16 with `dwaveloop` and then
immediately issues `ach_prop 16,%sfx_vol,700`; user state showed `%sfx_vol=10`
while the legacy engine SFX channel volume was still 100. The shared
`dwaveCommand()` path now detects an immediate timed same-channel `ach_prop` and
primes the new sample to volume 0 before playback so the script performs a
fade-in to `%sfx_vol` instead of a fade-down from stale engine volume.

Status: build-verified on the local Windows UCRT64 SDL3 build and copied to
`D:\Umineko Project\onscripter-ru.exe`. Runtime listening validation of the
updated splash behavior is pending.

### Text and Sprite Rendering Hot Paths

The 2026-06-04 text/sprite rendering audit focused on lower per-frame CPU,
allocation, and transient GPU-target pressure without changing playback quality
or draw order.

Dialogue rendering now traverses the existing segment/run/piece deques directly
instead of constructing temporary pointer deques for every render and fade tick.
Text passes also avoid a renderable glyph cache lookup until the pass is known
to draw, and `renderGlyphValues()` returns immediately for fully transparent
glyphs before touching atlas/image state. Visible glyph draws still resolve the
current renderable glyph at draw time so late button-cell color changes and
shadow styling remain correct.

Sprite setup now fills `spriteZLevels` directly from LSP/LSP2 arrays instead of
first materializing an ordered sprite set that is immediately reinserted by
z-level. Transformed sprite canvas checkout is delayed until after
special-scrollable, big-image, layer, and missing-image early exits. Scaled
big images delay temporary canvas checkout until after clipping proves the image
will be drawn, and fully opaque big-image chunks no longer issue redundant
`GPU_SetRGBA(..., 255)` set/reset calls.

Status: build-verified on the local Windows UCRT64 SDL3 build and copied to
`D:\Umineko Project\onscripter-ru.exe`. No benchmark or runtime telemetry pass
was run for this rebuild.

### Branding and Renderer Display Name

The 2026-06-04 branding pass changed the generated default executable target
from `onscripter-ru` to `onscripter-new`, updated the current generated UCRT64
makefile accordingly, and updated Windows resource metadata so the original
filename and product/file descriptions match `onscripter-new.exe`.

The window title is now pinned to
`Umineko no Naku Koro ni: ~Rondo of the Witch and Reasoning~` at startup and
when scripts issue `caption`, avoiding the previous appended engine/version
caption. The single SDL3_GPU renderer remains the Vulkan SDL_GPU backend, but
its user-facing renderer name is now `Vulkan`; existing `SDL3_GPU`
`prefer-renderer` and `renderer-blacklist` values are still accepted as legacy
aliases.

Status: build-verified on the local Windows UCRT64 SDL3 build and copied to
`D:\Umineko Project\onscripter-new.exe`. No benchmark or runtime telemetry pass
was run for this rebuild.

### Menu Responsiveness and Refresh Pacing

The 2026-06-04 menu responsiveness pass targets Config entry latency, Config
navigation overhead, Music Box BGM switching latency, and GUI animation pacing.

Config again constructs all pages at their normal offscreen positions before
navigation, avoiding first-visit delays and page-transition artifacts. To keep
entry responsive, the startup menu cache now asynchronously prewarms the 46
voice-toggle character icon surfaces used by Config page two, so `lsp` can
reuse decoded surfaces when Config constructs the offscreen pages. The Config
menu also has a `Restart Game` action for restart-required settings. The latest
bottom-row layout pairs `Reset Progress` and `Restart Game` on the left, and
uses the former right-side restart slot for a `Controls` button.

Music Box normal BGM selections now use a `bgmfast` command. It loads the new
music while the current track remains active, then releases the old music only
after the replacement has loaded successfully. SDL3_mixer music loads also use
streaming `MIX_Audio` instead of predecoding the entire track during selection,
which targets the underlying delay instead of only masking the stop/load gap.

Default GUI/event frame pacing now queries the active window display refresh
rate through SDL and recreates the frame-time generator when the reported rate
changes. The bundled script's `setfps 60` no longer pins the GUI to 60 Hz when a
valid display refresh rate is available; `force-fps` remains the explicit
config override. Invalid or extreme display rates fall back to the script FPS
when present, then the legacy 30 FPS default.

The follow-up animation-pacing pass moves the core `waitEvent()` frame baseline
from millisecond SDL ticks to SDL's high-resolution performance counter, resets
stale accumulated overshoot after long non-render gaps or refresh-rate changes,
and keeps dynamic-property interpolation in nanoseconds instead of quantizing
through `Clock::time()`. This targets menu movements such as the Tips panel,
where stale frame debt or millisecond pacing could collapse visible updates
below the monitor refresh cadence. The packed text cursor assets still use
10-cell sprite strips, so the renderer now cross-fades the current and next
cursor cells every rendered frame while leaving ordinary sprite-sheet
animations discrete.

Status: the local UCRT64 build linked successfully without warning output and
was copied to `D:\Umineko Project\onscripter-new.exe`. The active English
script was repacked to `D:\Umineko Project\en.file` and decode round-trip
verified before copy. A 12 second startup smoke test kept the process alive
until it was intentionally terminated. No benchmark, menu timing capture, or
longer runtime telemetry pass was run for this change.

Follow-up status: the animation-pacing rebuild linked successfully and was
copied to `D:\Umineko Project\onscripter-new.exe`. A 12 second startup smoke
test kept the process alive until it was intentionally terminated. No benchmark,
menu timing capture, or longer runtime telemetry pass was run for this
follow-up.

The 2026-06-05 animation follow-up keeps the cursor on the refresh-paced path
but changes its smoothing from two-cell cross-fading to drawing a single full
cursor cell with a smoothly computed alpha phase. This avoids normal alpha
blending overbrightening the white interior when two partially transparent
cursor cells overlap. The title Tips submenu animation was also moved out of
the script's old `msp` plus `waittimer 50` stepping pattern and into dynamic
`spt` `ypos`/`alpha` properties over the same 350 ms motion window, so the
engine can update the Tips button movement every display-paced frame.

Second follow-up status: the local UCRT64 build linked successfully. The active
English script was repacked and decode round-trip verified, then the updated
`onscripter-new.exe` and `en.file` were copied to `D:\Umineko Project`. A
12 second startup smoke test kept the process alive until it was intentionally
terminated. No benchmark, menu timing capture, or longer runtime telemetry pass
was run for this follow-up.

The second 2026-06-05 animation follow-up corrects the cursor smoothing against
the actual packed cursor sheets: cell 0 is the bright/full cell and later cells
fade downward, so the renderer now draws cell 0 with an alpha phase matching
that original direction. This keeps the monitor-paced cursor fade without
making the sprite faint. The remaining title-menu button open animations for
Tea Party, Ura Tea/????, and Characters now use dynamic `spt` `ypos`/`alpha`
properties instead of the old `msp` plus `waittimer` stepping blocks, matching
the prior Tips fix.

Third follow-up status: the local UCRT64 build linked successfully. The active
English script was repacked and decode round-trip verified, then the updated
`onscripter-new.exe` and `en.file` were copied to `D:\Umineko Project`. A
12 second startup smoke test kept the process alive until it was intentionally
terminated. No benchmark, menu timing capture, or longer runtime telemetry pass
was run for this follow-up.

The fourth 2026-06-05 animation follow-up moves Config page scrolling off the
old per-sprite submission shape. The new `spriterangept` command stores a
temporary range offset as a dynamic custom property, redraws the HUD once per
display-paced frame, and commits the final offset to the affected sprite
positions after `spriterangeptwait`. The Config script now animates the
`190`-`384` page-control range with one grouped property while the existing
`config_main_lsp2` background/container animation remains on `aspt2`.

Fourth follow-up status: the local UCRT64 build linked successfully. The active
English script was repacked and decode round-trip verified, then the updated
`onscripter-new.exe` and `en.file` were copied to `D:\Umineko Project`. A
12 second startup smoke test kept the process alive until it was intentionally
terminated. No benchmark, menu timing capture, or longer runtime telemetry pass
was run for this follow-up.

The 2026-06-05 Config reset UI text/layout pass changes only the packed script:
the Config reset button now reads `Reset Progress`, the destructive-reset
prompt and choices use the requested copy, and the confirmation layout centers
the bounded prompt plus a padded centered choice row. The active English script
was repacked and decode round-trip verified. `make -j8` reported the binary
target was already current; the current `onscripter-new.exe` and updated
`en.file` were copied to `D:\Umineko Project`. No benchmark, menu timing
capture, or longer runtime telemetry pass was run for this script-only update.
The immediate follow-up changes only the reset confirmation choice aliases so
`Yes, I'm sure` and `No!` use the same white-normal/red-hover scheme as the
Config `Reset Progress` button. The active English script was repacked and
decode round-trip verified again. `make -j8` reported the binary target was
already current; the current `onscripter-new.exe` and updated `en.file` were
copied to `D:\Umineko Project`. No benchmark, menu timing capture, or longer
runtime telemetry pass was run for this script-only color update.

The follow-up Config polish pass also changes only the packed script: visible
Config setting titles are title-cased, Effect/Voice slider rows use the same
title-to-slider vertical spacing as BGM, and the textbox window selector now
shows a scaled live preview from the existing `msgwnd` assets. The active
English script was repacked and decode round-trip verified again. `make -j8`
reported the binary target was already current; the current
`onscripter-new.exe` and updated `en.file` were copied to `D:\Umineko Project`.
No benchmark, menu timing capture, or longer runtime telemetry pass was run
for this script-only UI update.

The immediate Config scroll-regression correction keeps the preview but moves
its sprites into the existing `190`-`384` Config page-control range. The extra
`spriterangept` calls for the preview-only range were removed because the
grouped range command tracks one active range per property, so the second call
could interrupt the main Config page movement. The active English script was
repacked and decode round-trip verified again. `make -j8` reported the binary
target was already current; the current `onscripter-new.exe` and fixed
`en.file` were copied to `D:\Umineko Project`. No benchmark, menu timing
capture, or longer runtime telemetry pass was run for this script-only
correction.

The follow-up textbox selector layout update remains script-only: the TypeN/
TypeB/TypeL/TypeT labels and sample preview sentence were removed, the live
textbox-window preview moved into the selector row, and left/right arrows now
cycle the selected window style using the same row-control pattern as Song
Subtitles. The active English script was repacked and decode round-trip
verified again. `make -j8` reported the binary target was already current; the
current `onscripter-new.exe` and updated `en.file` were copied to
`D:\Umineko Project`. No benchmark, menu timing capture, or longer runtime
telemetry pass was run for this script-only layout update.

The textbox preview spacing/assets follow-up spreads the selector arrows beyond
the preview width, scales and lowers the preview for padding from Automode
Speed, and uses preview-only cropped PNGs derived from the existing textbox
window assets so detached left-side decoration pieces are not shown in Config.
The active English script was repacked and decode round-trip verified again.
`make -j8` reported the binary target was already current; the current
`onscripter-new.exe` and updated `en.file` were copied to
`D:\Umineko Project`. No benchmark, menu timing capture, or longer runtime
telemetry pass was run for this UI asset/layout update.

The final textbox preview arrow alignment pass changes only packed-script
coordinates: both arrow sprites are vertically centered against the preview
row, and the right arrow is moved inward so its gap from the preview matches
the left arrow. The active English script was repacked and decode round-trip
verified again. `make -j8` reported the binary target was already current; the
current `onscripter-new.exe` and updated `en.file` were copied to
`D:\Umineko Project`. No benchmark, menu timing capture, or longer runtime
telemetry pass was run for this coordinate-only update.

The empty textbox preview label follow-up changes only packed-script text: the
no-window textbox style now displays centered `No Window` text in its preview
area instead of appearing blank. The active English script was repacked and
decode round-trip verified again. `make -j8` reported the binary target was
already current; the current `onscripter-new.exe` and updated `en.file` were
copied to `D:\Umineko Project`. No benchmark, menu timing capture, or longer
runtime telemetry pass was run for this script-only label update.

The immediate `No Window` alignment follow-up changes only the packed-script
label y coordinate so the no-window text uses the same vertical row coordinate
as the selector arrows. The active English script was repacked and decode
round-trip verified again. `make -j8` reported the binary target was already
current; the current `onscripter-new.exe` and updated `en.file` were copied to
`D:\Umineko Project`. No benchmark, menu timing capture, or longer runtime
telemetry pass was run for this coordinate-only update.

The 2026-06-06 pause-menu UI pass changes only the in-game pause-menu script
layout/text and active game-directory PNG artwork. The `Clear` button artwork
now reads `Hide`, session labels are title-cased and right-aligned in the
lower-right corner as separately measured text sprites, and the episode/chapter
artwork has been relocated above the left-side `Load` button. The active
English script was repacked and decode round-trip verified, the UCRT64 release
executable relinked with no warning lines in the captured output, and the
updated executable/script/artwork were copied to `D:\Umineko Project`. No
benchmark, menu timing capture, runtime telemetry, or executable boot test was
run for this UI/layout update.

The immediate pause-menu hover/artwork follow-up keeps the same layout but
resets all pause-menu button sprites to their normal cells before the
right-click loop re-registers them, preventing a stale hover cell from
surviving when `Hide` returns to the menu. The active `Hide` button strip was
rebuilt from the original artwork backup with normal-state text color matched
to the other buttons and one shared cleaned background across normal and hover
cells. The active English script was repacked and decode round-trip verified;
`make -j8` reported the binary target was already current, and the current
executable plus updated script/artwork were copied to `D:\Umineko Project`. No
benchmark, menu timing capture, runtime telemetry, or executable boot test was
run for this UI/artwork correction.

The second pause-menu follow-up restores the original active `Clear` button
strip from its saved backup, keeps the lower-right session-info placement while
reducing that text to a smaller dedicated preset. The active English script was
repacked and decode round-trip verified; `make -j8` reported the binary target
was already current, and the current executable plus updated script/restored
artwork were copied to `D:\Umineko Project`. No benchmark, menu timing capture,
runtime telemetry, or executable boot test was run for this UI/script
correction.

The emergency pause-menu input correction restores the original async
right-click-menu button-wait loop after a script-side no-result rebuild attempt
blocked hover and click input. The active English script was repacked and
decode round-trip verified, the UCRT64 release executable rebuilt
successfully, and the updated executable/script were copied to
`D:\Umineko Project`. No benchmark, menu timing capture, runtime telemetry, or
executable boot test was run for this input correction.

The pause-menu hover root-cause follow-up leaves the restored async menu loop
unchanged and removes the live `_csp` calls from `*rmenu_draw_time`. The
right-aligned session-info split had been clearing and recreating four normal
sprites on every async polling pass; `cspCommand()` also clears normal-sprite
button hover bookkeeping, so the engine lost the previous hovered button link
before mouse-leave could reset its cell. The session lines now use explicit
sprite aliases and are updated in place with `lsp`/`amsp`. The failed engine
hover repaint experiments were reverted, the active English script was
repacked and decode round-trip verified, the UCRT64 release executable rebuilt
successfully, and the updated executable/script were copied to
`D:\Umineko Project`. No benchmark, menu timing capture, runtime telemetry, or
executable boot test was run for this script/hover correction.

The session-label follow-up changes only packed-script pause-menu text: when
no BGM track is active, the `Current Track` session line now renders `None`
instead of an empty value. The active English script was repacked and decode
round-trip verified; `make -j8` reported the binary target was already current,
and the current executable plus updated script were copied to
`D:\Umineko Project`. No benchmark, menu timing capture, runtime telemetry, or
executable boot test was run for this text-only correction.

The Tips/Characters detail follow-up changes only packed-script menu UI:
Tips/Grimoire tab anchors are corrected against their visible pixel bounds, and
Characters Execute/Resurrect string buttons are redrawn under the right
information panel with persistent red selected-state labels derived from the
selected character condition. The active English script was repacked and decode
round-trip verified; `make -j8` reported the binary target was already current,
and the current executable plus updated script were copied to
`D:\Umineko Project`. No benchmark, menu timing capture, runtime telemetry, or
executable boot test was run for this UI correction.

The Config controls layout and polish follow-ups change only packed-script
Config UI: `Restart Game` now sits to the right of `Reset Progress`, and a new
`Controls` button occupies the former right-side restart slot. The button opens
a modal keyboard/mouse/gamepad keybind reference, with keybind groups
color-coded in gold and descriptions left white. The latest correction splits
the `Controls` header into its own centered text sprite, keeps the listing
centered, removes the dedicated Backlog section, and keeps the listing body on
foreground sprite `3` so sprite `5`'s dim overlay does not occlude it. Because
the listing uses a fixed `1700px` text box, its body sprite is now pinned at
`x=110`, and the width tag wraps the full popup body so each line uses that
same centered wrap area. It returns to the existing Config loop
without changing page state. The active English script was repacked and decode
round-trip verified after each pass; `make -j8` reported the binary target was
already current, and the current executable plus updated script were copied to
`D:\Umineko Project`. No benchmark, menu timing capture, runtime telemetry, or
executable boot test was run for these UI corrections.

### Renderer Telemetry

The SDL3_GPU backend now has opt-in shutdown telemetry. Enable it with:

```sh
./onscripter-new.exe --sdl3-gpu-telemetry
```

or:

```sh
ONS_SDL3_GPU_TELEMETRY=1 ./onscripter-new.exe
```

The telemetry logs aggregate command-buffer submissions, texture uploads,
readbacks, native fixed-pipeline draws/vertices, native shader draws/vertices,
CPU blit pixels, CPU shader fallback pixels, and per-shader native/fallback
compile and draw counts. It also logs per-source texture upload/readback rows
so full-surface transfers can be attributed to video frame uploads, subtitle
uploads, surface copy-out, glyph atlas `simulateRead()`, CPU fallback paths,
image copy/update paths, clears, and multiply-alpha updates. It is intended for
representative playthroughs, where the next remaining CPU shader fallback can
be identified by shader name and pixel count, and the largest transfer sources
can be ranked by byte count.

### Readback/Upload Source Telemetry

Transfer telemetry now has scoped source buckets layered on top of the existing
aggregate counters. The source scope prefers the outermost active label, so
higher-level work such as `video_frame_nv12`, `video_frame_yuv420p`,
`video_frame_surface`, `video_frame_alpha_mask`, `subtitle_frame`,
`subtitle_image_set`, `glyph_simulate_read`, `cpu_blit_fallback`, and
`cpu_shader_fallback` remains visible even when it flows through generic
renderer helpers such as `GPU_UpdateImageBytes()` or `GPU_CopyImage()`.

This does not reduce transfer volume by itself, but it makes the next runtime
telemetry pass actionable: the full-surface readbacks and large upload buckets
can be ranked by source before changing renderer semantics.

### Split Full-Clear Upload Telemetry

The former `clear_full` transfer bucket now splits fallback uploads by cause and
dimensions. Full-image `GPU_ClearRGBA()` calls that fail or cannot use the
native render-pass clear report as
`clear_full_native_fallback_<width>x<height>`. Clipped or virtual-resolution
clears that must preserve pixels outside the clear bounds report as
`clear_full_clipped_upload_<target>_rect_<rect>`.

This keeps the existing aggregate upload accounting intact while making the
next runtime log identify whether the dominant clear uploads come from native
clear failure, target-size mismatches, or clipped virtual-resolution clears.

### Native Clipped Clear Rectangles

Clipped `GPU_ClearRGBA()` calls no longer have to synchronize the CPU mirror and
upload the clear rectangle. When the clear bounds do not cover the physical
target texture, the backend now draws a native solid rectangle into the target
using the fixed SDL3_GPU pipeline and a 1x1 white sampler texture. This keeps
the GPU pixels outside the clear bounds intact, marks the CPU mirror dirty for
future explicit readback, and avoids the immediate preserve-readback/upload
pair.

The CPU preserve-and-upload path remains as a fallback. If it is ever used, its
pre-readback and upload now share the split clear telemetry label instead of
leaving the readback in the generic `ensure_pixels_current` bucket.

### Glyph Atlas simulateRead GPU Copy

The SDL3 `simulateRead()` workaround no longer initializes its temporary atlas
image with `GPU_CopyImage()`. That path synchronized the source image's CPU
mirror and then uploaded the copy. The temporary atlas image is now created with
matching dimensions and populated through a GPU-to-GPU blit, which preserves the
workaround while avoiding that initial CPU readback/upload pair.

### Shutdown Texture Lifetime Tracking

The SDL3_GPU backend now tracks every `GPU_Image` that owns an SDL_GPU texture.
Normal image destruction, target backing resize, mipmap texture recreation, and
renderer shutdown all release through the same helper. During `GPU_Quit()`, any
remaining live image textures are released before `SDL_DestroyGPUDevice()`.

This targets the Vulkan validation leaks from the previous telemetry runs,
where unreleased `VkImage` and `VkImageView` children survived until device
destruction. The 2026-06-02 shutdown-lifetime telemetry run no longer reported
those validation leaks in `out.txt` or `err.txt`.

### PNG Save Longjmp Warning Cleanup

The PNG screenshot/save helper now performs surface conversion, optional
surface locking, and row-pointer setup before entering libpng's `setjmp`
write helper. Cleanup is explicit on both successful writes and libpng error
returns, including unlocking any locked surface. The local UCRT64 rebuild no
longer emits the previous GCC `-Wclobbered` longjmp warnings from
`saveSurfacePNG_RW()`.

### SDL3 Warning-Clean Release Build

The 2026-06-03 UCRT64 warning-clean release rebuild no longer emits the
remaining GCC diagnostics from SDL3 joystick ID validation, display-mode
fallback initialization, SDL3-disabled touch gesture handling, PNG load
longjmp state, or unused video framerate counting. The clean build was copied
to `D:\Umineko Project\onscripter-ru.exe`.

The root `README.md` was also refreshed during this pass to identify
`onscripter-new` as the Umineko Project ONScripter-RU modernization branch and
to point readers at the local compilation, dependency, and SDL3 performance
status documents.

### Leaf Dependency Refresh

The 2026-06-03 dependency refresh replaced IJG jpeg 9c with libjpeg-turbo
3.1.4.1, updated Lua to 5.4.8, and updated libusb to 1.0.30. SDL3_image 3.4.4
was rebuilt against the new static `jpeg` provider during the local UCRT64
release build. The engine linked without `warning:` lines and the updated
executable was copied to `D:\Umineko Project\onscripter-ru.exe`.

### Umineko Project Startup/Video Telemetry

Runtime pass:

- Executable: `D:\Umineko Project\onscripter-ru.exe`
- Arguments: `--use-logfile --sdl3-gpu-telemetry`
- Working directory: `D:\Umineko Project`
- Log file: `C:\ProgramData\ONScripter-RU\out.txt`
- Scenario: boot through the opening logo/video path for 45 seconds, then send
  a normal window close and allow renderer shutdown to flush telemetry.

Aggregate telemetry:

| Counter | Value |
| --- | ---: |
| Command buffers submitted | 32,738 |
| Texture uploads | 6,255 |
| Texture upload bytes | 13,341,253,452 |
| Readbacks | 1,903 |
| Readback bytes | 36,351,786,592 |
| Native fixed-pipeline draws | 10,082 |
| Native fixed-pipeline vertices | 50,016 |
| Native shader compiles | 4 |
| Compatibility shader compiles | 15 |
| Native shader draws | 2,937 |
| Native shader vertices | 11,748 |
| CPU blit draws/pixels | 0 / 0 |
| CPU shader draws/pixels | 704 / 579,731 |

Per-shader highlights:

| Shader | Native compiles | Native draws | CPU fallback draws | CPU fallback pixels |
| --- | ---: | ---: | ---: | ---: |
| `blendByMask.frag` | 1 | 866 | 0 | 0 |
| `blurH.frag` / blur path | 2 | 962 | 0 | 0 |
| `colourConversion.frag` | 1 | 1,109 | 0 | 0 |
| `colorModification.frag` | 0 | 0 | 186 | 191,805 |
| `glyphGradient.frag` | 0 | 0 | 518 | 387,926 |

The boot/video path confirms that the current hot shaders for mask blending,
logo blur, and YUV video conversion are executing natively. The remaining CPU
shader work in this path is small, about 0.58 megapixels total, and is limited
to `colorModification.frag` and `glyphGradient.frag`. Those fallbacks should be
ported eventually, but they are not the dominant cost in this measured path.

The dominant renderer pressure is now CPU/GPU synchronization and data
movement: about 36.35 GB of readback traffic and 13.34 GB of texture upload
traffic were recorded. The average readback was about 19.1 MB, which points to
full-surface downloads rather than small readback rectangles. Source buckets
have now been added so the next runtime pass can separate CPU shader
synchronization, glyph atlas `simulateRead()` calls,
`GPU_CopySurfaceFromImage()`/save paths, subtitle uploads, and video/frame
upload paths.

Shutdown also emitted Vulkan validation errors for unreleased `VkImage` and
`VkImageView` children at `vkDestroyDevice`. This indicates that some SDL3_GPU
textures/views survive until device destruction. It is not a frame-time
performance issue, but it is a renderer lifetime correctness issue that should
be fixed before removing the SDL2 fallback.

### Umineko Project Source-Tagged Startup/Video Telemetry

Runtime pass:

- Executable: `D:\Umineko Project\onscripter-ru.exe`
- Arguments: `--use-logfile --sdl3-gpu-telemetry`
- Working directory: `D:\Umineko Project`
- Log file: `C:\ProgramData\ONScripter-RU\out.txt`
- Scenario: user-controlled boot/video telemetry run, closed normally by the
  user after roughly one minute so renderer shutdown could flush telemetry.

Aggregate telemetry:

| Counter | Value |
| --- | ---: |
| Command buffers submitted | 23,673 |
| Texture uploads | 3,050 |
| Texture upload bytes | 4,321,236,476 |
| Readbacks | 1,122 |
| Readback bytes | 28,060,115,776 |
| Native fixed-pipeline draws | 7,689 |
| Native fixed-pipeline vertices | 34,212 |
| Native shader compiles | 4 |
| Compatibility shader compiles | 15 |
| Native shader draws | 2,073 |
| Native shader vertices | 8,292 |
| CPU blit draws/pixels | 0 / 0 |
| CPU shader draws/pixels | 704 / 579,731 |

Per-source transfer telemetry:

| Source | Uploads | Upload bytes | Readbacks | Readback bytes | Share of upload bytes | Share of readback bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `clear_full` | 425 | 3,043,471,316 | 0 | 0 | 70.43% | 0.00% |
| `video_frame_yuv420p` | 984 | 1,020,211,200 | 0 | 0 | 23.61% | 0.00% |
| `update_image` | 878 | 137,421,580 | 0 | 0 | 3.18% | 0.00% |
| `multiply_alpha` | 59 | 117,813,456 | 0 | 0 | 2.73% | 0.00% |
| `cpu_shader_fallback` | 704 | 2,318,924 | 704 | 23,622,320,128 | 0.05% | 84.18% |
| `ensure_pixels_current` | 0 | 0 | 418 | 4,437,795,648 | 0.00% | 15.82% |

Per-shader highlights:

| Shader | Native compiles | Native draws | CPU fallback draws | CPU fallback pixels |
| --- | ---: | ---: | ---: | ---: |
| `blendByMask.frag` | 1 | 783 | 0 | 0 |
| `blurH.frag` / blur path | 2 | 962 | 0 | 0 |
| `colourConversion.frag` | 1 | 328 | 0 | 0 |
| `colorModification.frag` | 0 | 0 | 186 | 191,805 |
| `glyphGradient.frag` | 0 | 0 | 518 | 387,926 |

Findings from source tagging:

- CPU shader fallback is now proven to be the dominant readback source in this
  path. It accounts for 704 readbacks and 23.62 GB, or 84.18% of all readback
  bytes. The average CPU-fallback readback is exactly 33,554,432 bytes
  (32 MiB), which means tiny fallback shader workloads are forcing full backing
  image synchronization.
- The generic `ensure_pixels_current` bucket is the remaining readback source,
  with 418 readbacks and 4.44 GB. Its average readback is about 10.62 MB, so it
  is also mostly large-surface synchronization rather than small rectangles.
- `clear_full` is the dominant upload source, with 3.04 GB and 70.43% of upload
  bytes. This bucket is the fallback branch of `GPU_ClearRGBA()` after the
  native clear path does not return, so this made a follow-up split necessary
  between non-covering virtual-resolution clears, target-size mismatches, and
  any native clear failure. Current builds now perform that split for future
  telemetry runs.
- YUV420P video plane uploads are visible and bounded: 984 uploads totaling
  1.02 GB, averaging 1,036,800 bytes per upload. That size is consistent with
  expected planar video traffic and is no longer the largest measured problem.
- The `glyph_simulate_read` scope did not appear in this run. That means the
  startup/video path did not use the VMware-style simulated read workaround
  during the measured interval, or it did not perform a transfer after the
  source scope was active.
- The same Vulkan shutdown leak remains: validation reported unreleased
  `VkImage` children and then hit the duplicate-message limit before reporting
  an unreleased `VkImageView`.

### Umineko Project Post-Glyph-SPIR-V Telemetry

Runtime pass:

- Executable: `D:\Umineko Project\onscripter-ru.exe`
- Arguments: `--use-logfile --sdl3-gpu-telemetry`
- Working directory: `D:\Umineko Project`
- Log file: `C:\ProgramData\ONScripter-RU\out.txt`
- Scenario: user-controlled boot/video telemetry run started at 2026-06-02
  17:39:30 and closed normally after roughly 46 seconds.

Aggregate telemetry:

| Counter | Value |
| --- | ---: |
| Command buffers submitted | 22,287 |
| Texture uploads | 2,158 |
| Texture upload bytes | 3,777,274,832 |
| Readbacks | 359 |
| Readback bytes | 3,811,408,224 |
| Native fixed-pipeline draws | 7,419 |
| Native fixed-pipeline vertices | 33,132 |
| Native shader compiles | 6 |
| Compatibility shader compiles | 13 |
| Native shader draws | 2,733 |
| Native shader vertices | 10,932 |
| CPU blit draws/pixels | 0 / 0 |
| CPU shader draws/pixels | 0 / 0 |

Transfer telemetry highlights:

| Source | Uploads | Upload bytes | Readbacks | Readback bytes | Share of upload bytes | Share of readback bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `clear_full_clipped_upload_*` total | 366 | 2,635,575,796 | 0 | 0 | 69.77% | 0.00% |
| `clear_full_clipped_upload_2172x1222_rect_1920x1080` | 302 | 2,504,908,800 | 0 | 0 | 66.32% | 0.00% |
| `video_frame_yuv420p` | 855 | 886,464,000 | 0 | 0 | 23.47% | 0.00% |
| `update_image` | 878 | 137,421,580 | 0 | 0 | 3.64% | 0.00% |
| `multiply_alpha` | 59 | 117,813,456 | 0 | 0 | 3.12% | 0.00% |
| `ensure_pixels_current` | 0 | 0 | 359 | 3,811,408,224 | 0.00% | 100.00% |

Additional clipped clear upload buckets:

| Clear bucket | Uploads | Upload bytes |
| --- | ---: | ---: |
| `clear_full_clipped_upload_2172x1222_rect_1745x500` | 11 | 38,390,000 |
| `clear_full_clipped_upload_2172x1222_rect_2046x1151` | 4 | 37,679,136 |
| `clear_full_clipped_upload_2172x1222_rect_750x182` | 37 | 20,202,000 |
| `clear_full_clipped_upload_2172x1222_rect_1745x546` | 4 | 15,244,320 |
| `clear_full_clipped_upload_2172x1222_rect_1965x1125` | 1 | 8,842,500 |
| `clear_full_clipped_upload_2172x1222_rect_1890x1098` | 1 | 8,300,880 |

Per-shader highlights:

| Shader | Native compiles | Native draws | CPU fallback draws | CPU fallback pixels |
| --- | ---: | ---: | ---: | ---: |
| `blendByMask.frag` | 1 | 782 | 0 | 0 |
| `blurH.frag` / blur path | 2 | 962 | 0 | 0 |
| `colorModification.frag` | 1 | 186 | 0 | 0 |
| `colourConversion.frag` | 1 | 285 | 0 | 0 |
| `glyphGradient.frag` | 1 | 518 | 0 | 0 |

Findings from the post-glyph-SPIR-V run:

- The embedded SPIR-V paths for `colorModification.frag` and
  `glyphGradient.frag` worked. The previous run had 704 CPU shader fallback
  draws and 579,731 fallback pixels; this run had zero CPU shader fallback
  draws and zero fallback pixels.
- Readback traffic fell from 28.06 GB to 3.81 GB, an 86.42% reduction from the
  previous source-tagged run. Readback count fell from 1,122 to 359.
- All remaining readback bytes are in the generic `ensure_pixels_current`
  bucket. The average readback is 10,616,736 bytes, exactly one 2172x1222 RGBA
  target. This matches the target dimensions reported by the split clipped
  clear labels.
- All split clear uploads are `clear_full_clipped_upload_*`; no
  `clear_full_native_fallback_*` bucket appeared. That means the large upload
  path is not native render-pass clear failure. It is clipped/virtual-resolution
  clearing that preserves pixels outside the clear rectangle.
- The dominant clear upload is 302 uploads of a 1920x1080 rectangle inside a
  2172x1222 target, totaling 2.50 GB and 66.32% of all upload bytes. The
  clipped clear path also likely explains most or all of the 359 full-target
  `ensure_pixels_current` readbacks because it must read the 2172x1222 target
  before uploading the clipped rectangle.
- YUV420P video traffic remains bounded and expected at 855 uploads totaling
  886.46 MB.
- The same Vulkan shutdown leak remains. Validation again reported unreleased
  `VkImage` children and then hit the duplicate-message limit before reporting
  an unreleased `VkImageView`.

### Umineko Project Post-Native-Clipped-Clear Telemetry

Runtime pass:

- Executable: `D:\Umineko Project\onscripter-ru.exe`
- Arguments: `--use-logfile --sdl3-gpu-telemetry`
- Working directory: `D:\Umineko Project`
- Log file: `C:\ProgramData\ONScripter-RU\out.txt`
- Scenario: user-controlled boot/video telemetry run started at 2026-06-02
  17:54:51 and closed normally.

Aggregate telemetry:

| Counter | Value |
| --- | ---: |
| Command buffers submitted | 21,840 |
| Texture uploads | 1,786 |
| Texture upload bytes | 1,135,478,236 |
| Readbacks | 0 |
| Readback bytes | 0 |
| Native fixed-pipeline draws | 7,715 |
| Native fixed-pipeline vertices | 34,316 |
| Native shader compiles | 6 |
| Compatibility shader compiles | 13 |
| Native shader draws | 2,733 |
| Native shader vertices | 10,932 |
| CPU blit draws/pixels | 0 / 0 |
| CPU shader draws/pixels | 0 / 0 |

Transfer telemetry:

| Source | Uploads | Upload bytes | Readbacks | Readback bytes | Share of upload bytes | Share of readback bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `video_frame_yuv420p` | 849 | 880,243,200 | 0 | 0 | 77.52% | 0.00% |
| `update_image` | 878 | 137,421,580 | 0 | 0 | 12.10% | 0.00% |
| `multiply_alpha` | 59 | 117,813,456 | 0 | 0 | 10.38% | 0.00% |

Per-shader highlights:

| Shader | Native compiles | Native draws | CPU fallback draws | CPU fallback pixels |
| --- | ---: | ---: | ---: | ---: |
| `blendByMask.frag` | 1 | 784 | 0 | 0 |
| `blurH.frag` / blur path | 2 | 962 | 0 | 0 |
| `colorModification.frag` | 1 | 186 | 0 | 0 |
| `colourConversion.frag` | 1 | 283 | 0 | 0 |
| `glyphGradient.frag` | 1 | 518 | 0 | 0 |

Findings from the post-native-clipped-clear run:

- The native clipped-clear path removed the remaining measured readbacks:
  readback count and bytes both fell from 359 / 3.81 GB to zero.
- The split clear upload buckets disappeared. Clear fallback uploads fell from
  366 uploads and 2.64 GB to zero.
- Total texture upload bytes fell from 3.78 GB to 1.14 GB, a 69.94% reduction
  from the post-glyph-SPIR-V run. Remaining upload traffic is now bounded by
  expected video plane uploads, image updates, and multiply-alpha updates.
- Native fixed-pipeline draws rose by 296 draws and 1,184 vertices, consistent
  with moving clipped clear rectangles from CPU preserve/upload into native
  four-vertex rectangle draws.
- CPU shader fallback remained zero.
- This run still reported Vulkan shutdown validation leaks for unreleased
  `VkImage` and `VkImageView` children. A later code change now tracks and
  releases remaining live image textures during `GPU_Quit()`, and the
  shutdown-lifetime verification run below no longer reports that validation
  output.

### Umineko Project Shutdown-Lifetime Verification Telemetry

Runtime pass:

- Executable: `D:\Umineko Project\onscripter-ru.exe`
- Arguments: `--use-logfile --sdl3-gpu-telemetry`
- Working directory: `D:\Umineko Project`
- Log file: `C:\ProgramData\ONScripter-RU\out.txt`
- Error log: `C:\ProgramData\ONScripter-RU\err.txt`
- Scenario: user-controlled telemetry run started at 2026-06-02 18:47:35 and
  closed normally after roughly 56 seconds.

Aggregate telemetry:

| Counter | Value |
| --- | ---: |
| Command buffers submitted | 45,668 |
| Texture uploads | 1,620 |
| Texture upload bytes | 918,653,504 |
| Readbacks | 4 |
| Readback bytes | 33,177,600 |
| Native fixed-pipeline draws | 31,923 |
| Native fixed-pipeline vertices | 544,192 |
| Native shader compiles | 6 |
| Compatibility shader compiles | 13 |
| Native shader draws | 1,701 |
| Native shader vertices | 6,804 |
| CPU blit draws/pixels | 0 / 0 |
| CPU shader draws/pixels | 0 / 0 |

Transfer telemetry:

| Source | Uploads | Upload bytes | Readbacks | Readback bytes | Share of upload bytes | Share of readback bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `update_image` | 1,306 | 467,493,904 | 0 | 0 | 50.89% | 0.00% |
| `multiply_alpha` | 250 | 403,856,992 | 0 | 0 | 43.96% | 0.00% |
| `copy_image` | 60 | 14,125,008 | 0 | 0 | 1.54% | 0.00% |
| `generate_mipmaps` | 4 | 33,177,600 | 0 | 0 | 3.61% | 0.00% |
| `ensure_pixels_current` | 0 | 0 | 4 | 33,177,600 | 0.00% | 100.00% |

Per-shader highlights:

| Shader | Native compiles | Native draws | CPU fallback draws | CPU fallback pixels |
| --- | ---: | ---: | ---: | ---: |
| `blendByMask.frag` | 1 | 589 | 0 | 0 |
| `blurH.frag` / blur path | 2 | 288 | 0 | 0 |
| `colorModification.frag` | 1 | 306 | 0 | 0 |
| `colourConversion.frag` | 1 | 0 | 0 | 0 |
| `glyphGradient.frag` | 1 | 518 | 0 | 0 |

Findings from the shutdown-lifetime verification run:

- The renderer shutdown texture-lifetime fix is runtime-verified for this
  scenario. `out.txt` contained no Vulkan validation, `VkImage`, `VkImageView`,
  `vkDestroyDevice`, or `VUID` leak output, and `err.txt` contained only two
  debugger-version warnings.
- CPU blit and CPU shader fallback remained at zero.
- The run recorded four readbacks totaling 33,177,600 bytes. The average
  readback was 8,294,400 bytes, exactly one 1920x1080 RGBA surface. The matching
  `generate_mipmaps` upload total suggests these were tied to mipmap generation,
  not to the previous clipped-clear or CPU-shader fallback paths.
- No `video_frame_yuv420p` bucket appeared and `colourConversion.frag` had zero
  native draws, so this run should not be compared directly with the earlier
  opening-video telemetry samples for video-plane traffic.

### Umineko Project Breakup-Path Telemetry

Runtime pass:

- Executable: `D:\Umineko Project\onscripter-ru.exe`
- Arguments: `--use-logfile --sdl3-gpu-telemetry`
- Working directory: `D:\Umineko Project`
- Log file: `C:\ProgramData\ONScripter-RU\out.txt`
- Error log: `C:\ProgramData\ONScripter-RU\err.txt`
- Scenario: user-controlled in-game telemetry run at a different gameplay
  point, closed normally at 2026-06-02 19:03:21.

Aggregate telemetry:

| Counter | Value |
| --- | ---: |
| Command buffers submitted | 126,066 |
| Texture uploads | 1,441 |
| Texture upload bytes | 4,374,929,136 |
| Readbacks | 367 |
| Readback bytes | 3,896,342,112 |
| Native fixed-pipeline draws | 101,038 |
| Native fixed-pipeline vertices | 3,132,016 |
| Native shader compiles | 6 |
| Compatibility shader compiles | 13 |
| Native shader draws | 1,617 |
| Native shader vertices | 6,468 |
| CPU blit draws/pixels | 0 / 0 |
| CPU shader draws/pixels | 357 / 70,648,268 |

Transfer telemetry:

| Source | Uploads | Upload bytes | Readbacks | Readback bytes | Share of upload bytes | Share of readback bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `cpu_shader_fallback` | 367 | 3,896,342,112 | 367 | 3,896,342,112 | 89.06% | 100.00% |
| `update_image` | 978 | 278,262,100 | 0 | 0 | 6.36% | 0.00% |
| `multiply_alpha` | 96 | 200,324,924 | 0 | 0 | 4.58% | 0.00% |

Per-shader highlights:

| Shader | Native compiles | Native draws | CPU fallback draws | CPU fallback pixels |
| --- | ---: | ---: | ---: | ---: |
| `alphaOutsideTextures.frag` | 0 | 0 | 357 | 70,648,268 |
| `blendByMask.frag` | 1 | 896 | 0 | 0 |
| `colorModification.frag` | 1 | 203 | 0 | 0 |
| `glyphGradient.frag` | 1 | 518 | 0 | 0 |

Findings from the breakup-path run:

- `alphaOutsideTextures.frag` was the only CPU shader fallback source in this
  scenario. It evaluated 70.65 MP over 357 draws.
- The fallback forced 367 full 2172x1222 RGBA synchronizations. The average
  fallback readback was 10,616,736 bytes, matching the previous full-target
  size seen in clipped-clear telemetry before that path was fixed.
- CPU shader fallback accounted for all readback bytes and 89.06% of upload
  bytes in this run.
- No Vulkan validation leak output appeared in `out.txt`, and `err.txt`
  contained only one debugger-version warning.
- The source now embeds `alphaOutsideTextures.frag` as native SPIR-V. The
  follow-up telemetry target is zero CPU fallback draws for this shader at the
  same breakup-heavy gameplay point.

### Umineko Project Post-Triangle-Batching Startup/Video Telemetry

Runtime pass:

- Executable: `D:\Umineko Project\onscripter-ru.exe`
- Arguments: `--use-logfile --sdl3-gpu-telemetry`
- Working directory: `D:\Umineko Project`
- Log file: `C:\ProgramData\ONScripter-RU\out.txt`
- Error log: `C:\ProgramData\ONScripter-RU\err.txt`
- Scenario: user-controlled startup/video telemetry run started at 2026-06-03
  15:46:37 and closed normally at 2026-06-03 15:47:18 after native indexed
  triangle/shader draw batching and full built-in SPIR-V coverage.

Aggregate telemetry:

| Counter | Value |
| --- | ---: |
| Command buffers submitted | 21,273 |
| Texture uploads | 1,287 |
| Texture upload bytes | 606,977,756 |
| Readbacks | 0 |
| Readback bytes | 0 |
| Native fixed-pipeline draws | 7,581 |
| Native fixed-pipeline vertices | 33,780 |
| Native shader compiles | 18 |
| Compatibility shader compiles | 1 |
| Native shader draws | 2,714 |
| Native shader vertices | 10,856 |
| CPU blit draws/pixels | 0 / 0 |
| CPU shader draws/pixels | 0 / 0 |

Transfer telemetry:

| Source | Uploads | Upload bytes | Readbacks | Readback bytes | Share of upload bytes | Share of readback bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `video_frame_yuv420p` | 336 | 348,364,800 | 0 | 0 | 57.39% | 0.00% |
| `update_image` | 885 | 139,110,540 | 0 | 0 | 22.92% | 0.00% |
| `multiply_alpha` | 66 | 119,502,416 | 0 | 0 | 19.69% | 0.00% |

Per-shader highlights:

| Shader | Native compiles | Native draws | CPU fallback draws | CPU fallback pixels |
| --- | ---: | ---: | ---: | ---: |
| `blendByMask.frag` | 1 | 936 | 0 | 0 |
| `blurH.frag` / blur path | 2 | 962 | 0 | 0 |
| `colorModification.frag` | 1 | 186 | 0 | 0 |
| `colourConversion.frag` | 1 | 112 | 0 | 0 |
| `glyphGradient.frag` | 1 | 518 | 0 | 0 |

Findings from the post-triangle-batching startup/video run:

- CPU blit and CPU shader fallback remained at zero.
- Readbacks remained at zero in this measured startup/video path.
- All built-in fragment shaders compiled natively in the Vulkan SPIR-V path;
  only the default vertex shader used the compatibility compile counter.
- The run is shorter than some earlier startup/video samples, so total command
  buffers should not be treated as a strict apples-to-apples reduction. The
  synthetic benchmark is the cleaner comparison for the triangle batching
  change.

### Umineko Project Main-Menu Resource Pass

Runtime passes:

- Executable: `D:\Umineko Project\onscripter-ru.exe`
- Arguments: `--use-logfile --sdl3-gpu-telemetry`
- Working directory: `D:\Umineko Project`
- Profile output:
  `DerivedData\profile-submit-throttle`,
  `DerivedData\profile-final-default-hw-on`,
  `DerivedData\profile-final-default-throttle8`
- Scenario: boot Umineko Project release data to the visible main menu and
  sample process memory/CPU while the menu video continues playing.

Fixes applied in this pass:

- Command-buffer submission now goes through a shared SDL3_GPU submit helper.
  The helper records telemetry and waits for the GPU after a small backlog of
  queued command buffers, preventing the SDL3/D3D12 private-memory backlog that
  previously grew past 6 GB during menu loading.
- `GPU_Image` CPU pixel mirrors are allocated lazily, clean mirrors can be
  discarded after upload, and `GPU_UpdateImageBytes()` uploads rows directly
  without keeping a persistent CPU copy of video planes.
- Decoded image caching now has a default 64 MiB budget configurable with
  `ONS_IMAGE_CACHE_MB`, and animation surfaces are freed after equivalent GPU
  images/big images exist unless the CPU surface is still required.
- The event-fetch thread now waits longer when idle, reducing idle polling.
- Hardware video decoding and hardware format conversion are enabled by default
  unless explicitly disabled with `--hwdecoder off` or `--hwconvert off`.

Validated menu observations:

- The visible menu screenshot at
  `DerivedData\profile-submit-throttle\attached-shot-090s.png` showed Task
  Manager reporting ONScripter-RU at 475.7 MB memory and 4.7% CPU.
- The default hardware-on menu screenshot at
  `DerivedData\profile-final-default-hw-on\screen-030s.png` showed Task Manager
  reporting ONScripter-RU at 410.0 MB memory and 4.6% CPU.
- The matching CSV samples in
  `DerivedData\profile-final-default-hw-on\perf-final-default-hw-on.csv`
  recorded 891.7 MB private / 464.9 MB working set / 4.05% CPU at 30 seconds
  and 969.7 MB private / 526.3 MB working set / 4.40% CPU at 60 seconds.
- Shutdown telemetry from the default hardware-on run showed the intended YUV
  plane upload path: `video_frame_yuv420p` recorded 624 uploads totaling
  690,094,080 bytes, with zero CPU blit/shader fallback and zero persistent CPU
  pixel mirrors for live images.
- The final UCRT64 build after tightening the command-buffer backlog cap linked
  successfully and was copied to `D:\Umineko Project\onscripter-ru.exe`.

### Video Decode Resource Pass

The 2026-06-04 video decode pass targets CPU, RAM, and GPU memory use in the
direct YUV/NV12 playback path without changing conversion quality or shader
output:

- Queued direct-conversion video frames now retain an FFmpeg `AVFrame`
  reference and point `MediaFrame::planes` at that storage. This removes the
  previous per-frame `new[]` allocations and full-plane `memcpy` before the
  unavoidable SDL_GPU transfer-buffer upload.
- Subtitle blending on retained YUV/NV12 frames calls `av_frame_make_writable()`
  only when subtitles are active, preserving the zero-copy queued-frame path for
  normal video playback and copying only if FFmpeg still shares the buffer.
- The decoded ready-frame queue has a smaller frame-specific cap
  (`12` desktop, `8` Android/iOS) while compressed packet buffering and
  startup timecode sampling retain their existing depth. This limits decoded
  frame RAM and decode-ahead CPU work without reducing demux packet buffering.
- Direct shader-converted videos no longer preallocate RGB staging surfaces.
  The staging pool still exists and allocates on demand if a stream or option
  falls back to SWS RGB conversion.
- Transient YUV plane textures and the alpha-mask helper texture are released
  as soon as playback finishes, including `LeaveCurrent` cases that keep the
  final RGB frame visible.
- `VideoDecoder::deinitSwsContext()` now nulls the freed SWS context pointer,
  avoiding stale reuse or double-free risk when a later frame or fallback path
  reinitializes software scaling.

Status: the local UCRT64 build linked successfully without warning output and
was copied to `D:\Umineko Project\onscripter-ru.exe`. No benchmark or runtime
telemetry pass was run for this change.

### Broad CPU Audit Pass

The follow-up 2026-06-04 CPU audit pass targets steady-state CPU overhead that
does not require changing visual output, audio quality, or script timing:

- The SDL event queues now store `SDL_Event` values directly. Normal SDL events
  are fetched into stack storage and copied into the queue, avoiding the
  previous heap allocation/free pair for each queued event and for every
  synthetic upkeep/batch-end marker.
- The async loop now skips unused results-semaphore posts for no-result queues.
  The SDL event fetcher idle timeout was briefly raised from 8 ms to 16 ms in
  this pass, then restored to 8 ms during the transition-smoothness follow-up.
- Temporary CPU image and PNG loader pools now maintain free lists. Checkout no
  longer scans the full `unordered_map` looking for an unused object, which
  reduces CPU in software-converted video frames and image loading.
- The temporary GPU image pool originally gained the same free-list treatment,
  but that change was later reverted for transition smoothness. GPU render
  targets again use the previous unordered-map scan reuse policy and
  clear-on-return behavior.
- SDL3 texture upload staging now uses a single contiguous copy when both the
  source pitch and GPU transfer pitch match the row width. The padded row path
  remains unchanged for uploads that need alignment padding.
- High-level shader program activation now caches the last alias pointer. This
  avoids repeated `std::string` construction and unordered-map lookup when the
  same literal shader alias is used repeatedly across frames, such as the video
  color-conversion shader and common transition/effect shaders.
- The video `AudioBridge` callback now writes decoded audio chunks at
  `raw + rawPos` and leaves the base pointer stable, avoiding incorrect
  cumulative pointer advancement when multiple decoded chunks fill one mixer
  buffer.

Status: the local UCRT64 build linked successfully without warning output and
was copied to `D:\Umineko Project\onscripter-ru.exe`. No benchmark or runtime
telemetry pass was run for this change.

### Transition Smoothness Follow-Up

After the broad CPU audit build, some transitions were observed to be choppy.
The likely transition-sensitive regressions were the doubled SDL event fetcher
idle timeout and the changed lifetime ordering for temporary GPU render target
clears.

The follow-up 2026-06-04 build restores the event fetcher idle timeout from
16 ms to the previous 8 ms cadence, and restores clear-on-return for temporary
GPU images while keeping free-list checkout. This keeps the lower-risk event
allocation, temporary pool, upload, shader alias cache, async semaphore, and
audio bridge improvements in place, but reverts the two changes most likely to
affect effect pacing or pooled render-target ordering.

Status: the local UCRT64 build linked successfully and was copied to
`D:\Umineko Project\onscripter-ru.exe`. No benchmark or runtime telemetry pass
was run for this change.

### Black Transition Follow-Up

Black transitions remained choppy after the initial transition-smoothness
follow-up. The remaining broad-audit change most specific to this path was the
temporary GPU image pool's immediate free-list reuse of recently returned
full-screen render targets.

The follow-up 2026-06-04 build restores the temporary GPU image pool's previous
unordered-map scan reuse policy and clear-on-return behavior. This avoids
changing the reuse order for full-screen canvas/script render targets used by
`bg black` fades while keeping the CPU-side surface and PNG-loader free lists.

Status: the local UCRT64 build linked successfully and was copied to
`D:\Umineko Project\onscripter-ru.exe`. No benchmark or runtime telemetry pass
was run for this change.

### RAM Audit Pass

The 2026-06-04 RAM audit pass targets retained CPU/GPU memory without changing
playback quality, shader output, or script-visible image content:

- The decoded image cache default is now 64 MiB instead of 256 MiB. The
  `ONS_IMAGE_CACHE_MB` override remains available, including `0` for the
  existing unlimited-cache behavior.
- Reusable SDL3_GPU staging buffers now grow to a 256 KiB-aligned capacity
  instead of the next power of two. This reduces retained upload/download,
  vertex, and index staging over-allocation while preserving reuse.
- Synchronous texture readbacks now release their reusable download transfer
  buffer immediately after the mapped pixels have been copied. The upload
  transfer buffer is still retained for normal frame-to-frame uploads because
  submitted GPU work may still reference it.

Status: the local UCRT64 build linked successfully and was copied to
`D:\Umineko Project\onscripter-ru.exe`. No benchmark or runtime telemetry pass
was run for this change.

### Audio Backend Audit Pass

The 2026-06-04 audio backend pass targets CPU and allocation overhead without
changing sample formats, fade behavior, channel layout, playback quality, or
adding compression:

- The SDL3_mixer compatibility `Mix_LoadWAV_RW()` path now decodes into the
  final SDL-managed PCM buffer with geometric `SDL_realloc()` growth. This
  removes the previous decode-vector plus second full-buffer copy before
  `MIX_LoadRawAudioNoCopy()`.
- Normal one-shot channel/music starts now pass `0` options to `MIX_PlayTrack()`
  and use SDL3_mixer defaults instead of allocating and destroying an SDL
  properties object for every non-looping play.
- `Mix_VolumeFloat()` and `Mix_VolumeMusicFloat()` now skip redundant
  `MIX_SetTrackGain()` calls when the clamped gain has not changed. Playback
  start still reapplies gain immediately after `MIX_PlayTrack()` to preserve
  the existing channel-restart fade fix.
- `playSoundThreaded()` now uses a blocking semaphore wait for calls that do
  not request event pumping. The 1 ms timeout loop remains only for the
  `waitevent` path that must keep dispatching engine events during a blocking
  sound load.
- Video audio frames that already match the active mixer spec now retain an
  FFmpeg `AVFrame` reference and hand its PCM data to `AudioBridge` directly.
  The resampling path is unchanged and still allocates converted output when
  the source format, sample rate, channel count, or layout differs.

Status: the local UCRT64 build linked successfully and was copied to
`D:\Umineko Project\onscripter-ru.exe`. No benchmark or runtime telemetry pass
was run for this change.

### Whole-Codebase Allocation and Lookup Pass

The 2026-06-06 audit targeted low-risk CPU overhead across the engine without
changing frame pacing, draw order, filtering, shader math, decoded audio
samples, or video frame contents:

- SAR/NSA archive tables now build a normalized filename index at archive-load
  time, replacing repeated linear scans and per-candidate string allocation.
  Vector reads from archive members now read directly into the caller buffer
  instead of allocating an intermediate buffer and copying it again.
- Text-window blit regions use fixed storage sized for the known split-window
  maximum, and dialogue traversal returns reserved pointer vectors instead of
  temporary deques. Layout code avoids repeated `getPieces()` calls when the
  same pointer list is reused.
- SDL event queues use `std::deque` value storage, and touch coalescing keeps
  the two pending finger events inline instead of allocating `SDL_Event`
  objects on the heap.
- Dynamic-property name tables are fixed arrays, property scans use unchecked
  fixed-table indexing, `StringTree` insertion does one map lookup, and cache
  controller/LRU paths avoid repeated map lookups or full key-list copies for
  single evictions.
- Native SDL3_GPU triangle batches now carry sampler bindings in fixed arrays
  and bind from stack storage during flush. `GPU_TriangleBatch()` reuses
  thread-local vertex scratch storage before queueing into the native batch,
  avoiding a heap allocation on repeated compatible triangle submissions.
- GPU shader/resource and temporary image-pool lookups now use single `find()`
  paths instead of `count()` plus `at()` or `operator[]` lookup pairs.
- Direct hardware-converted FFmpeg video frames are retained with
  `av_frame_clone()`, using the updated FFmpeg API to create the referenced
  frame directly.

Status: three local UCRT64 `make -j8` rebuilds after the engine changes linked
successfully, and the rebuilt `onscripter-new.exe` was copied to
`D:\Umineko Project`. No benchmark, runtime telemetry, or executable boot test
was run for this source-level performance pass.

### C++23 Source Cleanup Follow-Up

The 2026-06-09 C++23 migration keeps renderer behavior unchanged while cleaning
up a few low-risk hot-path helpers: manual clamp expressions in
`SDL3GPUCompat.cpp` now use `std::clamp`, shader-program existence checks use
associative-container `contains()`, and sprite z-order map checks use `find()`
instead of `count()` followed by `operator[]`.

Status: the local UCRT64 public-release rebuild with `--std=gnu++23` linked
successfully with no warning output after the libusb enum-mask cleanup. No
benchmark, runtime telemetry, or executable boot test was run for this
language-level cleanup.

### Medium Whole-Codebase Performance Pass

The same-date medium pass targets CPU, RAM, and upload/readback pressure while
leaving frame pacing, wait timing, shader output, audio output, and script
visible behavior unchanged:

- Subtitle decode/display queues now use SDL semaphores for decoded-data and
  freed-space wakeups instead of 1 ms producer/consumer spin-sleeps. The
  subtitle timestamp state is updated under the queue mutex so the renderer and
  decoder threads no longer race on `decoded_timestamp`.
- Full-destination `GPU_UpdateImage()` calls with matching source/destination
  pixel size now stream the SDL surface rows directly into the reusable
  SDL3_GPU transfer buffer. The CPU mirror is not built for those full uploads;
  partial updates and conversion-heavy updates keep the existing synchronized
  CPU mirror path.
- Full-image `GPU_MultiplyAlpha()` on SDL3 now falls through to the existing
  `multiplyAlpha.frag` shader path, which has embedded native SPIR-V coverage,
  instead of doing the premultiply loop on the CPU. Clipped premultiply keeps
  the old CPU path because small rectangles can be cheaper than a full-image
  temporary render pass.
- `GPU_DiscardImagePixels()` now discards stale CPU mirrors when the GPU
  texture is authoritative, reducing retained RAM after shader writes.
- Mipmap texture recreation now creates the replacement mipmapped texture and
  copies mip level 0 GPU-to-GPU before `SDL_GenerateMipmapsForGPUTexture()`.
  The previous CPU synchronization/reupload path remains as fallback.

Status: the local UCRT64 `make -j8` rebuild linked successfully without warning
output, and the rebuilt `onscripter-new.exe` was copied to
`D:\Umineko Project\onscripter-new.exe`. No benchmark, runtime telemetry, or
executable boot test was run for this source-level performance pass.

### SDL3 Gamepad Input Follow-Up

The 2026-06-09 controller support pass moves mapped controllers onto SDL3's
normalized gamepad events, while raw SDL joystick input remains a fallback for
unmapped devices. Hotplug is handled through SDL joystick/gamepad add/remove
events, duplicate raw joystick events are ignored for devices opened as SDL
gamepads, and known Sony DualShock 3/4 vendor/product IDs normalize the raw
fallback GUID path when SDL does not supply a recognized GUID.

The normalized map fixes the observed DualShock 4 fallback issue where Cross
could arrive as button 0 and trigger the old generic mute binding. Cross now
maps to confirm, Circle to cancel, Share/Back to mute, Options/Start to the
tab/message-browser action, and stick clicks are left unmapped. Left and right
sticks keep menu navigation but use a higher press threshold plus release
hysteresis to avoid jitter-driven repeats.

Status: the local UCRT64 `make -j8` rebuild linked successfully, and the rebuilt
`onscripter-new.exe` was copied to `D:\Umineko Project\onscripter-new.exe`. No
benchmark, runtime telemetry, or executable boot test was run for this
input-routing pass.

Follow-up: SDL3 gamepad button and axis events were added to the shared
`inputEventList` used by dialogue rendering, wait actions, and button monitors.
The first pass translated normalized gamepad events in the default event pass,
which allowed global side effects such as Share/Back mute but did not wake the
active script wait for text advance or button selection. The follow-up keeps
normalized gamepad events on the same wait path as keyboard and raw joystick
events. The UCRT64 rebuild linked successfully and the corrected executable was
copied to `D:\Umineko Project\onscripter-new.exe`; no executable boot test was
run.

Config input-hints follow-up: the packed English script now treats the former
`DualShock` selector as an input-hints selector, not a controller enable
switch. The row is labeled `Input Hints`, the gamepad side is labeled
`Gamepad`, and the Controls popup switches between keyboard/mouse and
controller binding text based on `control_interface`. The script passed an
exact `nscmake`/`nscdec` round-trip, the binary target was already current, and
the updated `en.file` plus current executable were copied to
`D:\Umineko Project`; no executable boot test was run.

Binding correction: L1 now maps through a dedicated gamepad automode scancode
instead of keyboard `A`, R2 trigger axes are ignored, and the raw generic
gamepad R2 map no longer routes to right Ctrl. Triangle/Y remains the
Message Browser/backlog binding, while Options/Start now shares the pause-menu
binding used by Square/X. The gamepad Controls popup text was updated to match.
The UCRT64 rebuild linked successfully, the packed script passed an exact
round-trip, and the updated executable plus `en.file` were copied to
`D:\Umineko Project`; no executable boot test was run.

L1 automode follow-up: the dedicated gamepad automode scancode now takes a
separate event path from Cross in `textbtnwait`. It enables automode and wakes
the active text-button wait by arming its automode timer/voice wait instead of
directly setting button result `0`, so L1 does not behave as a one-sentence
advance. The UCRT64 rebuild linked successfully, the rebuilt executable was
copied to `D:\Umineko Project`, and no executable boot test was run.

Music Box/debug unlock script follow-up: this pass only changes the decoded
English script. Music Box entries that play opening videos are now labeled with
`(Video)` in every title language table, and a `debug_unlock_all` forced config
key plus hidden title-screen `C` shortcut unlocks episodes, chapter read flags,
character state, omake state, Music Box, CGs, and trophies without writing a
numbered save slot. The script repacked and round-tripped exactly; `make -j8`
reported the binary target was already current, and the current executable plus
updated `en.file` were copied to `D:\Umineko Project`. No executable boot test,
benchmark, or runtime telemetry was run.

Debug unlock coverage correction: this script-only follow-up makes
`debug_unlock_all` unlock the same Music Box IDs and Picture Box slots that the
UIs enumerate, including Rondo BGM IDs 94 and 1011, display-only Rondo CG slots
57 through 59, and Chiru-only Music Box entries. The script repacked and
round-tripped exactly; `make -j8` reported the binary target was already
current, and the current executable plus updated `en.file` were copied to
`D:\Umineko Project`. No executable boot test, benchmark, or runtime telemetry
was run.

Music Box selection-display follow-up: this script-only pass stores localized
BGM titles in the existing Music Box scrollable tree and shows the active normal
BGM selection in a centered footer sprite using the existing BGM-title
music-note styling and a short slide-in. Video entries clear the footer before
launching playback because they stop looping Music Box BGM. The script repacked
and round-tripped exactly; `make -j8` reported the binary target was already
current, and the current executable plus updated `en.file` were copied to
`D:\Umineko Project`. No executable boot test, benchmark, or runtime telemetry
was run.

Music Box footer visibility correction: this script-only pass moves the footer
display from story-layer sprite `748` to Music Box-local sprite `304`, which
draws above the menu overlay, scrollable list, and footer controls. It also
forces an immediate repaint after creating or clearing the footer sprite. The
script repacked and round-tripped exactly; `make -j8` reported the binary
target was already current, and the current executable plus updated `en.file`
were copied to `D:\Umineko Project`. No executable boot test, benchmark, or
runtime telemetry was run.

Picture Box variant-badge follow-up: this script-only pass overlays small
outlined count sprites on unlocked Picture Box thumbnails that open multiple
gallery images. The counts follow the actual Rondo/Chiru gallery jump targets,
and the overlay sprite IDs are included in the existing thumbnail page-slide
animation. The script repacked and round-tripped exactly; `make -j8` reported
the binary target was already current, and the current executable plus updated
`en.file` were copied to `D:\Umineko Project`. No executable boot test,
benchmark, or runtime telemetry was run.

Picture Box badge placement correction: this script-only follow-up widens the
badge text sprite, adds explicit border padding around the outlined digit
sprite to avoid right/bottom clipping, and moves the shared badge anchor almost
flush with each thumbnail's bottom-right corner. The script repacked and
round-tripped exactly; `make -j8` reported the binary target was already
current, and the current executable plus updated `en.file` were copied to
`D:\Umineko Project`. No executable boot test, benchmark, or runtime telemetry
was run.

Trophies text/color follow-up: this script-only pass removes the extra manual
description-line breaks from the two long playtime/click-count trophy entries,
updates the Platinum trophy description to the requested PERFECTO text, and
changes the Platinum rarity text to a cooler PSN-style blue-white. The script
repacked and round-tripped exactly; `make -j8` reported the binary target was
already current, and the current executable plus updated `en.file` were copied
to `D:\Umineko Project`. No executable boot test, benchmark, or runtime
telemetry was run.

Title loading/title caption follow-up: the title loading screen now queues its
image prewarm work with `async_cache_img` and drains it with a new
`cache_wait_img` command that pumps `waitEvent(0)` between async completions,
so the visible loading sprite animation is no longer blocked by each
synchronous cache load. The companion `cache_wait_snd` command shares the same
queue-drain path for future script use. The engine caption hardening now maps
recognized Rondo/Chiru caption requests to pinned titles, including
`Umineko no Naku Koro ni: ~Nocturne of Truth and Illusions~` for Chiru, while
the Platinum trophy description color-codes only `<IT'S PERFECTO>!` red. The
script repacked and round-tripped exactly; the UCRT64 rebuild linked
successfully, and the rebuilt executable plus updated `en.file` were copied to
`D:\Umineko Project`. No executable boot test, benchmark, or runtime telemetry
was run.

Miscellaneous UI/text/title follow-up: the 2026-06-11 pass updates the two
requested long trophy descriptions and changes the pinned Chiru window title to
the `Umineko no Naku Koro ni Chiru: ~Nocturne of Truth and Illusions~` string.
The attempted Config alignment coordinate changes from that pass were removed
in an immediate follow-up, restoring the prior Input Hints, Song Subtitles, and
Textbox Window positions. The script repacked and round-tripped exactly; the
UCRT64 `make -j8` command reported the binary target was already current, and
the current executable plus updated `en.file` were copied to
`D:\Umineko Project`. No executable boot test, benchmark, or runtime telemetry
was run.

Release-audit cleanup: the 2026-06-12 pass fixes stale external IDE helper
script output names after the `onscripter-new` rename, removes the stale
`.gitignore` entry for the tracked dependency audit document, and removes two
byte-identical macOS app-icon PNG duplicates by reusing the existing same-size
asset files. It also requires current `onscripter-new` engine outputs for
Android APK packaging, removes misleading async queue comments, updates current
benchmark/telemetry examples to the renamed executable, and removes ignored
helper-tool object files. It also removes an unused sprite-helper method and
trims a dead parameter from the remaining sprite helper API. The verification
follow-up updates runtime-visible branding strings and fixes Android
cross-build portability in libusb-gated joystick helpers, explicit standard
library includes, the Droid profiler atomic stop path, and SDL2-only
touch-threshold declarations. The script repacked and round-tripped exactly;
the UCRT64 release build relinked successfully; the Android arm64-v8a/x86_64
release build and APK packaging passed; APK signature, badging, permissions,
archive contents, and native library hashes were verified; and the rebuilt
Windows executable plus updated `en.file` were copied to `D:\Umineko Project`.
No executable boot test, benchmark, or runtime telemetry was run.

Verification-screen text follow-up: the 2026-06-13 script-only pass title-cases
the file verification header and shortens the success-path copy to
`Verifying files. This may take several minutes...` followed by
`All good! Press right click to continue.` The follow-up alignment pass moves
the header to center it over the gray verification panel and restores 15 px
border padding when the final verification result text sprite is regenerated,
preventing outlined descenders such as the `g` in `good` from being clipped.
The active English script repacked and decode round-tripped exactly after each
pass, then the updated `en.file` was copied to `D:\Umineko Project`. The game
config was re-armed with `env[verify]=once` for the next manual verification
test. No executable rebuild, boot test, benchmark, or runtime telemetry was
run.

Pause-menu and Message Browser follow-up: the 2026-06-13 script-only pass moves
the right-click pause-menu chapter-title sprite up by 180 internal pixels so
the white chapter art, such as `Episode 2`, no longer overlaps the live session
statistics. The Message Browser line-jump confirmation now reads `Are you sure
you want to jump to this line? This might take time.` and uses the centered
message/button alignment helper. The active English script repacked and decode
round-tripped exactly, then the updated `en.file` was copied to
`D:\Umineko Project`; `ons.cfg` was re-armed with `env[verify]=once`. No
right-click menu PNG assets, executable rebuild, boot test, benchmark, or
runtime telemetry were changed/run. The next release package needs this updated
`en.file` so fresh installs get the pause-menu and line-jump fixes.

Release documentation/package follow-up: the README now includes a dedicated
comparison of `onscripter-new` and ONScripter-RU, covering project scope,
branding, renderer/media stack, target platform floors, release data, and
runtime UX differences. Local `v2026.06.13` release artifacts were prepared
under `DerivedData\Release\v2026.06.13`: the Windows x86_64 zip keeps the
current `onscripter-new.exe`, includes the updated packed `en.file`, and
preserves the existing install notes; the Android APK was carried forward
unchanged because no native Android code changed; `SHA256SUMS.txt` was
regenerated and validated. No executable rebuild, boot test, benchmark, or
runtime telemetry was run for this docs/release-packaging pass.

Textbox preview asset repair: the 2026-06-17 active game-data pass regenerates
the loose Config textbox preview PNGs under
`D:\Umineko Project\graphics\system\wnd` from the existing `msgwnd` assets:
`msgwnd_preview_en.png`, `msgwnd_preview_ep5_en.png`, `msgwnd_preview2.png`,
`msgwnd_preview3.png`, and transparent `msgwnd_preview4.png`. The packed script
was already current; `D:\Umineko Project\en.file` matched
`DerivedData\decoded-script\en.file.new` by SHA-256, so no script repack,
executable rebuild, boot test, benchmark, or runtime telemetry was run.
The immediate crop correction regenerates those same preview PNGs right-aligned
to the original 1648 px textbox assets, preserving the source right edge and
border at the existing 1125x288 preview dimensions. No script repack,
executable rebuild, boot test, benchmark, or runtime telemetry was run.

FPS overlay keybind follow-up: the 2026-06-17 engine pass adds an `Alt+F`
in-game FPS counter toggle while preserving plain `F` as the fullscreen toggle
and keeping the existing `--show-fps` title-bar counter. The overlay draws as a
small cached screen-target element after scene/HUD/cursor composition, forces
refreshes while visible, and forces one cleanup refresh when disabled so stale
counter pixels are cleared. The UCRT64 `make -j8` rebuild linked successfully,
and the rebuilt `onscripter-new.exe` was copied to `D:\Umineko Project`. No
script repack, executable boot test, benchmark, or runtime telemetry was run.

Release packaging follow-up: local `v2026.06.17` artifacts were prepared under
`DerivedData\Release\v2026.06.17`. The Windows x86_64 zip includes the rebuilt
`onscripter-new.exe`, current packed `en.file`, refreshed install notes, and
the fixed loose textbox preview assets at
`graphics\system\wnd\msgwnd_preview_en.png`,
`msgwnd_preview_ep5_en.png`, `msgwnd_preview2.png`,
`msgwnd_preview3.png`, and transparent `msgwnd_preview4.png`. The staged and
extracted preview PNGs hash-match and remain 1125x288. The Android APK was
carried forward unchanged from `v2026.06.13` because current Android build
outputs/signing environment were not present locally. `SHA256SUMS.txt` was
regenerated for the Windows zip and carried-forward APK. No executable boot
test, benchmark, or runtime telemetry was run for this release packaging pass.

Release packaging follow-up: local `v2026.06.18` artifacts were prepared under
`DerivedData\Release\v2026.06.18`. The Windows x86_64 zip includes the rebuilt
144 Hz frame-pacing/text-rendering executable, current packed `en.file`,
install notes, and the maintained loose textbox preview assets at
`graphics\system\wnd\msgwnd_preview_en.png`,
`msgwnd_preview_ep5_en.png`, `msgwnd_preview2.png`,
`msgwnd_preview3.png`, and transparent `msgwnd_preview4.png`. The Android APK
was carried forward unchanged from `v2026.06.17`. `SHA256SUMS.txt` was
regenerated and validated for the Windows zip and carried-forward APK. The
release executable was build-verified with UCRT64 `make -j8`; SDL3 synthetic
and Music Box benchmark outputs from the final FreeType face-size fix are
recorded above.

Characters-menu polish follow-up: the 2026-06-18 packed-script UI pass swaps
the Characters action row to show `Resurrect` before `Execute`, renames the
outfit toggle to `Change Outfit`, centers that outfit action under the right
description panel both when it appears beside the death-state actions and when
it appears by itself, and title-cases the visible `Human Side`, `Witch Side`,
and `Ange Side` side selectors. The English script was repacked and decode
round-trip verified exactly. UCRT64 `make -j8` relinked successfully, and the
rebuilt `onscripter-new.exe` plus updated `en.file` were copied to
`D:\Umineko Project`. The Characters caption PNG was left unchanged after the
follow-up request to skip PNG edits. No executable boot test, benchmark, or
runtime telemetry was run.

Tips Controls text follow-up: the 2026-06-18 packed-script UI pass changes the
Tips `Controls` popup to read `If arrows are present, you can scroll text using
the mouse wheel or two-finger swipe gesture.` followed by the same explicit
close instruction used by the Config controls popup. The popup no longer closes
on a seven-second timer; it now waits for click, Enter/Space, right-click, or
Esc using a local button result so dismissing the popup does not exit the Tips
menu. The English script was repacked and decode round-trip verified exactly.
UCRT64 `make -j8` reported the binary target was already current, and the
current executable plus updated `en.file` were copied to `D:\Umineko Project`.
No executable boot test, benchmark, or runtime telemetry was run.

Tips Controls centering follow-up: the 2026-06-18 packed-script UI pass wraps
the Tips `Controls` popup copy in the same fixed-width centered text block used
by the Config controls popup, so both visible lines are centered within the
overlay instead of left-aligned inside the string sprite. The English script
was repacked and decode round-trip verified exactly. UCRT64 `make -j8`
reported the binary target was already current, and the current executable plus
updated `en.file` were copied to `D:\Umineko Project`. No executable boot test,
benchmark, or runtime telemetry was run.

Tips text arrow-gutter follow-up: the 2026-06-18 packed-script UI pass reduces
the English Tips/Grimoire description preset wrap width from 830 to 760 pixels.
Those right-panel descriptions still render from `x=930`, while the 90-pixel
scroll arrows are centered at `x=1740`; the narrower wrap leaves a right-side
gutter so long text lines cannot run under the top scroll arrow. The English
script was repacked and decode round-trip verified exactly. UCRT64 `make -j8`
reported the binary target was already current, and the current executable plus
updated `en.file` were copied to `D:\Umineko Project`. No executable boot test,
benchmark, or runtime telemetry was run.

Config Reset Progress confirmation follow-up: the 2026-06-18 packed-script UI
pass leaves the footer `Reset Progress` label unchanged but updates the
confirmation overlay that appears after selecting it. The reset prompt now uses
an exact-width text sprite instead of the old 1600-pixel centered wrapper, and
the prompt/buttons are positioned through the same shared centered-message
helper used by the jump confirmation dialog. The English script was repacked
and decode round-trip verified exactly. UCRT64 `make -j8` reported the binary
target was already current, and the current executable plus updated `en.file`
were copied to `D:\Umineko Project`. No executable boot test, benchmark, or
runtime telemetry was run.

Config Reset Progress confirmation spacing follow-up: the 2026-06-18
packed-script UI pass keeps the reset prompt centered through the shared modal
helper, then reapplies a reset-specific 75-pixel gap between `Yes, I'm sure`
and `No!` so the options no longer sit too close together. The English script
was repacked and decode round-trip verified exactly. UCRT64 `make -j8` reported
the binary target was already current, and the current executable plus updated
`en.file` were copied to `D:\Umineko Project`. No executable boot test,
benchmark, or runtime telemetry was run.

Jump-forward UI unknown-time follow-up: the 2026-06-18 packed-script UI pass
replaces the empty jump-date solid line with `??? --:--`. The jump-menu clock
now keeps a bright face at sprite `606` and a darkened face copy at sprite
`605`; handless/default entries leave the darkened copy visible, while all
timed clock aliases hide it before showing the hour and minute hands. The
English script was repacked and decode round-trip verified exactly. UCRT64
`make -j8` reported the binary target was already current, and the current
executable plus updated `en.file` were copied to `D:\Umineko Project`. No
executable boot test, benchmark, or runtime telemetry was run.

Jump-forward UI audio/portrait follow-up: the 2026-06-18 script pass expands
the unknown jump-date label to `?????? --:--`. The static jump-menu clock now
uses the existing clock ME branch in `display_clock3`; the current menu call
uses `umilse_011.ogg` through ME number 11 for the slow static clock tick, while
the animated story clock routines keep their existing 1050/1051 behavior.
Direct Ogg/Vorbis header inspection showed `umilse_011.ogg` is about 32.091
seconds, compared with about 9.503 seconds for `umilse_1050.ogg`, 34.088
seconds for `umilse_1051.ogg`, and 30.061 seconds for the rejected
`umilse_1053.ogg` chime candidate. The menu tick is stopped with `E_M5` on
title/back exits, at the selected-jump `perform_magic_jump` entry, and in the
common `scenario_jump_exit_efe1` effect path. The renderer now explicitly keeps
big-image chunks and the pooled transformed big-image canvas on linear
filtering, which gives the jump portraits the best no-asset scaling path
available. The episode portrait sheets are still 640x360 cells displayed at
175%, so this can smooth scaling but cannot restore detail that is not in the
source assets. The English script was repacked and decode round-trip verified
exactly. UCRT64 `make -j8` rebuilt and relinked `onscripter-new.exe`, and the
rebuilt executable plus updated `en.file` were copied to `D:\Umineko Project`
with matching SHA-256 hashes. A later script-only follow-up repacked the
English script after the 011 swap and copied the updated `en.file` to
`D:\Umineko Project` with matching SHA-256 hashes. No executable boot test,
benchmark, or runtime visual/audio pass was run.

Message Browser Controls hint follow-up: the 2026-06-18 packed-script UI pass
wraps `log_hint_text` in the same fixed-width centered text block style used by
Tips, adds the explicit close instruction, and replaces the seven-second
`wait 7000` timer in `*log_hint` with an input-dismiss `btnwait2`. The overlay
now remains visible until click/keyboard dismissal while preserving the Message
Browser button setup underneath. The English script was repacked and decode
round-trip verified exactly. UCRT64 `make -j8` reported the binary target was
already current, and the updated `en.file` was copied to `D:\Umineko Project`
with matching SHA-256 hashes. No executable boot test, benchmark, or runtime
visual pass was run.

Message Browser Controls hint centering follow-up: the 2026-06-18 packed-script
UI pass applies a local 71-pixel left adjustment after `align_message_l` in
`*log_hint`, correcting the measured rightward visual offset in the multi-line
Message Browser controls overlay without changing the shared alignment helper
or the Tips controls overlay. The English script was repacked and decode
round-trip verified exactly. UCRT64 `make -j8` reported the binary target was
already current, and the updated `en.file` was copied to `D:\Umineko Project`
with matching SHA-256 hashes. No executable boot test, benchmark, or runtime
visual pass was run.

Message Browser chapter-label/window-title follow-up: the 2026-06-18 UI pass
title-cases the Message Browser footer labels to `Previous Chapter` and
`Next Chapter`. The window controller now preserves a title set before renderer
creation and reapplies it when binding to the real renderer window, fixing the
SDL3 GPU launch path where the created window stayed on the fallback
`onscripter-new` title. The English script was repacked and decode round-trip
verified exactly. UCRT64 `make -j8` rebuilt and relinked
`onscripter-new.exe`; the rebuilt executable plus updated `en.file` were copied
to `D:\Umineko Project` with matching SHA-256 hashes. A short launch probe saw
the fallback title at 0.5 seconds and the expected Rondo title at 1.0 seconds.
No broader runtime visual/audio pass was run.

Discord Rich Presence follow-up: the 2026-06-19 engine pass adds a
dependency-free local Discord RPC IPC client for desktop builds. It stays
inactive unless a Discord Application ID is provided with runtime
`--discord-app-id`, `ONS_DISCORD_APP_ID`, or configure-time
`--discord-app-id=...`; `--disable-discord-rich-presence` suppresses it even
when an ID is configured. Presence sends basic details, a session start
timestamp, and a state derived from the pinned Rondo/Chiru window title when
`caption` runs, then explicitly clears the activity during normal shutdown.
Unsupported platforms, missing Discord desktop IPC, and missing/invalid app IDs
are no-op paths, and configure-time ID validation mirrors the runtime snowflake
length check. The current local release build embeds Discord Application ID
`1517334948967747794`, so end users do not need a command-line flag for Rich
Presence. The Xcode app targets also reference the new support source so Apple
builds link the implementation. UCRT64
`./configure --release-build --strip-binary --std=gnu++23 --discord-app-id=1517334948967747794`
succeeded, forced UCRT64 `make -B -j8` rebuilt and relinked
`onscripter-new.exe`, and the rebuilt executable was copied to
`D:\Umineko Project\onscripter-new.exe` with matching SHA-256 hash
`026EB812596397EAB6C69A4DEE726AC7FB5F244D3EDD16D7E6923A32250B577A`. The
executable help output showed the new Discord options. `git diff --check`
reported only the existing LF/CRLF working-copy warnings. No live Discord
client presence verification, executable boot test, benchmark, or broader
runtime telemetry pass was run.

Discord Rich Presence keepalive follow-up: the same-date correction keeps the
local Discord RPC connection serviced after the initial update. The client now
caches the current activity, polls the IPC pipe during frame upkeep, replies to
Discord ping frames, handles close/error frames, retries connection when Discord
is unavailable or restarted, and refreshes the current activity periodically.
This fixes the first-pass one-shot update behavior that could connect but fail
to remain visible. A direct local IPC probe against the running Discord desktop
client returned successful `READY`, `SET_ACTIVITY`, and clear responses for
Application ID `1517334948967747794`; a 25-second debug boot of
`D:\Umineko Project\onscripter-new.exe --use-console --debug` logged
`Discord Rich Presence connected` and `Discord Rich Presence activity accepted`.
UCRT64 configure with embedded Discord Application ID
`1517334948967747794` succeeded, UCRT64 `make -j8` relinked, and the rebuilt
executable was copied to `D:\Umineko Project\onscripter-new.exe` with matching
SHA-256 hash
`2924351CE44D278AD23E26268D007D55670DE69FC0085D45775E67CF4D3D1F8E`.
`git diff --check` still only reported the existing LF/CRLF working-copy
warnings. No manual Discord profile visual check or broader gameplay telemetry
pass was run.

Discord Rich Presence icon follow-up: the same-date asset pass adds optional
large image fields to the local RPC activity payload and points the default
activity at the existing 1024x1024 project icon hosted at
`https://raw.githubusercontent.com/timftw21/onscripter-new/master/Resources/Bundle/Images.xcassets/AppIcon-ios.appiconset/1024.png`.
A direct local Discord IPC probe using that URL returned a successful
`SET_ACTIVITY` response with a Discord media-proxied `large_image`; probing the
unuploaded `onscripter-new` asset key showed Discord dropping `large_image`,
confirming that an external URL avoids a required Developer Portal art-asset
upload. UCRT64 configure with embedded Discord Application ID
`1517334948967747794` succeeded, UCRT64 `make -j8` relinked, and the rebuilt
executable was copied to `D:\Umineko Project\onscripter-new.exe` with matching
SHA-256 hash
`09326BB533D1FF2C9E7D770EEDBFF1A44BD08B478201685C1D75FD70A96712ED`. A
20-second debug boot logged `Discord Rich Presence connected` and
`Discord Rich Presence activity accepted`; no manual Discord profile visual
check or broader gameplay telemetry pass was run.

Discord Rich Presence transparent icon follow-up: the same-date asset tweak
switches the default large-image URL from the 1024x1024 iOS app icon, whose PNG
has an opaque black background, to the existing 512x512 macOS app icon at
`https://raw.githubusercontent.com/timftw21/onscripter-new/master/Resources/Bundle/Images.xcassets/AppIcon-mac.appiconset/512x512.png`.
The macOS icon file has transparent corners (`Format32bppArgb`, alpha 0 at the
sampled corners) and Discord accepted the URL in a direct local IPC probe,
returning a media-proxied `large_image`. UCRT64 configure with embedded Discord
Application ID `1517334948967747794` succeeded, UCRT64 `make -j8` relinked,
and the rebuilt executable was copied to `D:\Umineko Project\onscripter-new.exe`
with matching SHA-256 hash
`5CFCF28807B25E384CACCDAD6A41959D69ABE59E874D9A6F399FC58AE7E62802`. A
20-second debug boot logged `Discord Rich Presence connected` and
`Discord Rich Presence activity accepted`; no manual Discord profile visual
check or broader gameplay telemetry pass was run.

Discord Rich Presence text follow-up: the same-date display tweak removes the
`details` line that previously showed `Reading Umineko Project` and wraps each
state string in tildes, e.g. `~Rondo of the Witch and Reasoning~`,
`~Nocturne of Truth and Illusions~`, or `~Starting up~`. A direct local Discord
IPC probe returned a successful `SET_ACTIVITY` response with the tilde-wrapped
state, the transparent icon `large_image`, and no `details` field. UCRT64
configure with embedded Discord Application ID `1517334948967747794` succeeded,
UCRT64 `make -j8` relinked, and the rebuilt executable was copied to
`D:\Umineko Project\onscripter-new.exe` with matching SHA-256 hash
`6D88646947D0A3167295A4F416D6048C234B60AC4D131913B5DA93573EB55963`. A
20-second debug boot logged `Discord Rich Presence connected` and
`Discord Rich Presence activity accepted`; no manual Discord profile visual
check or broader gameplay telemetry pass was run.

Chiru Picture Box eve_last panorama follow-up: the same-date script pass fixes
Chiru Picture Box page 5 slot 54, whose thumbnail uses
`graphics\thumb\chiru\eve_last.png`. The full-view handler now defines
`eve_last` as a `stralias`, loads it through `lbg2` so the `chiru.file`
`eve_last_left=11840` hotspot is honored, starts the strip at the left edge,
and pans to the registered right-end crop over 18 seconds before waiting for
the normal click/right-click exit. This replaces the previous static
`bg ":c;graphics\cg\cg_box\eve_last.png"` path, which ignored the hotspot and
treated the 12800x1080 panorama as an ordinary background. The English script
was repacked with `Tools/nscmake/nscmake.exe`, decoded back with
`Tools/nscdec/nscdec.exe`, and the decoded text matched exactly. The packed
`en.file.new` was copied to `D:\Umineko Project\en.file` with matching SHA-256
hash `CACEA2BEBD33C296F653D41939C1B47B6BB7AF665B0238DAAB8137BA54640417`.
A 12-second hidden debug startup loaded the updated script, initialized the
renderer and audio, and was then stopped manually; no direct Picture Box visual
navigation pass was run.

Discord Rich Presence Config toggle follow-up: the same-date privacy pass adds
a script command, `discord_presence`, and a Game Settings row directly under
Song Subtitles labelled `Discord Rich Presence`. The setting defaults on to
preserve the current behavior, writes `env[discord_presence]=true/false` to
`ons.cfg` through `operate_config u_write`, and applies immediately: off clears
the current Discord activity and shuts down the IPC client, while on reconnects
using the embedded Application ID when available. Startup now reads the same
user config value before connecting, so a saved false value suppresses the
initial Discord connection without requiring PowerShell or a command-line flag.
The command-line `--disable-discord-rich-presence` switch remains a hard
override. The English script was repacked with `Tools/nscmake/nscmake.exe`,
decoded back with `Tools/nscdec/nscdec.exe`, and the decoded text matched
exactly. UCRT64 `make -j8` rebuilt and relinked `onscripter-new.exe`; the
rebuilt executable and packed script were copied to `D:\Umineko Project` with
matching SHA-256 hashes
`7951F2CA32B22E0B313C5043FA62DD580FD56675907B746592C8DDB99145C27E` for
`onscripter-new.exe` and
`7956A3E8EB8191F2CD7BC7666A391B06554B0AB7C782C5E834184952BFE76746` for
`en.file`. An 18-second normal debug boot logged `Discord Rich Presence
connected` and `Discord Rich Presence activity accepted`; an 18-second debug
boot with `--env[discord_presence] false` initialized renderer and audio
without any Discord connection or activity logs. `git diff --check` reported
only the existing LF/CRLF working-copy warnings. No direct in-menu visual
navigation pass was run.

Release packaging follow-up: local `v2026.06.19` artifacts were prepared under
`DerivedData\Release\v2026.06.19`. The Windows x86_64 zip includes the rebuilt
Discord Rich Presence executable, the current packed `en.file` containing the
Picture Box panorama and Config-toggle script updates, install notes, and the
maintained loose textbox preview assets at
`graphics\system\wnd\msgwnd_preview_en.png`,
`msgwnd_preview_ep5_en.png`, `msgwnd_preview2.png`,
`msgwnd_preview3.png`, and transparent `msgwnd_preview4.png`. The release
archive was validated with `unzip -t`, and `SHA256SUMS.txt` was regenerated
for `onscripter-new-windows-x86_64.zip` with SHA-256
`03c6e8e913457d22ecb0e5d71678d57808e1e38a9d155cdc704fc1dc6ab65996`. A new
Android APK was not produced because the Android SDK/signing environment was
not present locally, and the previous APK was intentionally not carried forward
because the updated packed script now calls the new `discord_presence` command.

In-game load-time pass: the 2026-06-20 UCRT64 rebuild improves numbered save
restore work by introducing save format `4.1` with sparse sprite tables for new
saves while keeping the legacy fixed-table reader for existing `4.0` saves.
New saves now serialize only sprite slots that carry state instead of always
writing all 1000 `lsp` and 1000 `lsp2` entries. During restore, identical
static image tags reuse an already restored image resource from the same load,
avoiding repeated decode/upload work for duplicate scene assets; string sprites,
layers, and big images stay on the existing independent paths.

Status: UCRT64 `make -j8` linked successfully, and the rebuilt executable was
copied to `D:\Umineko Project\onscripter-new.exe` with matching SHA-256
`58C66EB1BEF72C0C26AB4D406140596DD3F0E0B1A5E9B29291B121D70F5EB026`.
Renderer benchmark output was written to
`DerivedData\MinGW-x86_64\sdl3-benchmark-after-sparse-save-load-final.csv`;
Music Box benchmark output was written to
`DerivedData\MinGW-x86_64\musicbox-benchmark-after-sparse-save-load-final.txt`
with `musicbox_full_frame_reordered` at 46.790 us/frame. A 15-second hidden
Umineko Project startup smoke initialized Vulkan, audio, script layers, and
the configured save path before the process was intentionally stopped.
`git diff --check` reported only the existing LF/CRLF working-copy warnings.
No direct in-menu save/load visual pass was run.

In-game load polish follow-up: the 2026-06-20 UCRT64 rebuild adds engine-owned
visual feedback around numbered save restores. `loadgame` now validates the save
header/checksum before changing the display, then presents the existing
four-frame `graphics\system\loading_en.png`/`loading_ru.png` strip in the
bottom-right corner while restore work is in progress. The overlay is drawn
through the HUD buffer so saved camera state cannot shift it, and restore
milestones step the animation without adding an artificial loading delay. Once
the save state is restored, the loading frame crossfades into the restored scene
instead of popping directly from black.

Status: UCRT64 `make -j8` linked successfully, and the rebuilt executable was
copied to `D:\Umineko Project\onscripter-new.exe` with matching SHA-256
`F1E51626208EE926A2522549902A0D205D4538788092B972CDCC8449E2B6FCEB`.
Quick SDL3 benchmark output was written to
`DerivedData\MinGW-x86_64\sdl3-benchmark-after-load-polish.csv`; Music Box
benchmark output was written to
`DerivedData\MinGW-x86_64\musicbox-benchmark-after-load-polish.txt` with
`musicbox_full_frame_reordered` at 46.145 us/frame. A 12-second hidden Umineko
Project startup smoke stayed alive until intentionally terminated; stderr only
reported the existing missing `en.cfg` warning. No direct in-menu load visual
pass was run because no local numbered save slot was found in the checked game
or user save locations.

Follow-up correction: the load overlay now always uses the English loading
strip for this build instead of consulting the legacy `english_mode` flag, which
can be false in the English Umineko Project script. The overlay is also pumped
from `waitEvent()` while save-load image work runs on the existing async image
queue, matching the main-menu loading animation pattern instead of advancing
only between restore milestones. The reveal is deferred through
`screenflip 1` from `loadgosub *load_system` (with a gosub-return fallback) so
script-side textbox and sprite-expression correction runs before the restored
scene crossfades in.

Status: UCRT64 `make -j8` linked successfully, and the rebuilt executable was
copied to `D:\Umineko Project\onscripter-new.exe` with matching SHA-256
`D273D62926F285F558DC4B4148E5118C1ABC4BDBF4C381BDAB38200E9151B41B`.
Quick SDL3 benchmark output was written to
`DerivedData\MinGW-x86_64\sdl3-benchmark-after-load-polish-followup.csv`;
Music Box benchmark output was written to
`DerivedData\MinGW-x86_64\musicbox-benchmark-after-load-polish-followup.txt`
with `musicbox_full_frame_reordered` at 53.065 us/frame. A 12-second hidden
Umineko Project startup smoke stayed alive until intentionally terminated;
stderr only reported the existing missing `en.cfg` warning.

Release packaging follow-up: local `v2026.06.21` artifacts were prepared under
`DerivedData\Release\v2026.06.21`. The Windows x86_64 zip includes the rebuilt
save-load performance/polish executable, the current packed `en.file`, install
notes, and the maintained loose textbox preview assets at
`graphics\system\wnd\msgwnd_preview_en.png`,
`msgwnd_preview_ep5_en.png`, `msgwnd_preview2.png`,
`msgwnd_preview3.png`, and transparent `msgwnd_preview4.png`. The release
archive was validated with `unzip -t`, and `SHA256SUMS.txt` was regenerated
for `onscripter-new-windows-x86_64.zip` with SHA-256
`2633a70b0ec1e760c9e74faad192b3bad7fa817c7c96bbacd320021a2b2bbaa9`.
No Android APK was produced because the Android SDK/signing environment was not
present locally.

Android release correction: local `v2026.06.21.1` artifacts were prepared under
`DerivedData\Release\v2026.06.21.1` to restore the Android APK after the
Discord Rich Presence script-command release. The APK was rebuilt from the
current engine source instead of carrying forward the pre-Discord APK, so the
Android build recognizes the `discord_presence` command while the unsupported
desktop IPC implementation stays a no-op on Droid. The shared
`Support\DiscordPresence` platform gate was centralized as
`ONS_DISCORD_PRESENCE_SUPPORTED`, removing the Droid-only unused-code warnings
from both arm64-v8a and x86_64 native builds.

Status: UCRT64 Windows release configure with embedded Discord Application ID
`1517334948967747794` and `make -j8` linked successfully. Android
`Scripts\quickdroid.tool --release` produced a multi-ABI APK containing
`lib/arm64-v8a/libmain.so` and `lib/x86_64/libmain.so`; the APK verified with
APK Signature Scheme v2, package id `org.umineko_project.onscripter_ru`, label
`onscripter-new`, min SDK 34, target SDK 36, only
`android.permission.VIBRATE`, and native code ABIs `arm64-v8a` and `x86_64`.
The Windows archive was validated with `unzip -t`. `SHA256SUMS.txt` now covers
both artifacts with SHA-256
`4d6b39cad201e2620854d7a3066035bc6330680d9f35e9b74e7f4169f1754d13` for
`onscripter-new-windows-x86_64.zip` and
`bc13b519c1293defaa8c78fc460b05d241eea0877dfad61e18a98143d939f9db` for
`onscripter-new-android.apk`. The refreshed Windows executable SHA-256 is
`C67474C7D4D5C3DC8AC18CE674131CFB262D75051351CF21881A69B9FB081156`.

Message Browser jump-confirmation centering follow-up: the 2026-06-22
packed-script UI pass removes the old 1600-pixel wrapper from
`jump_hint_text` so the shared centered-message helper measures and centers the
visible prompt text directly. This matches the exact-width approach already
used by the reset confirmation dialog and keeps the `Yes, I'm sure`/`No!`
choice row centered as a group. The English script was repacked and decode
round-trip verified exactly. The updated `en.file` was copied to
`D:\Umineko Project` with matching SHA-256
`56935E5F35063EB29E4A0C1B38CD2B8CA44A7FB98DC38AB56D96B83469C1B9C3`.
No executable rebuild, boot test, benchmark, or runtime visual pass was run.

Release packaging follow-up: local `v2026.06.22` artifacts were prepared under
`DerivedData\Release\v2026.06.22`. The Windows x86_64 zip carries forward the
current `onscripter-new.exe`, install notes, and maintained textbox preview
assets, and replaces only the packed `en.file` with the centered
Message Browser jump-confirmation script. The Android APK was carried forward
unchanged from `v2026.06.21.1` because the APK does not bundle `en.file` and no
native Android code changed. The Windows archive was validated with `unzip -t`;
the carried APK verified with APK Signature Scheme v2, package id
`org.umineko_project.onscripter_ru`, label `onscripter-new`, min SDK 34,
target SDK 36, only `android.permission.VIBRATE`, and native code ABIs
`arm64-v8a` and `x86_64`. `SHA256SUMS.txt` covers both artifacts with SHA-256
`019dba5f49871a2f0c4085bf4620fb7579dea4efc6152380d258ae16082fe14c` for
`onscripter-new-windows-x86_64.zip` and
`bc13b519c1293defaa8c78fc460b05d241eea0877dfad61e18a98143d939f9db` for
`onscripter-new-android.apk`. No executable rebuild, boot test, benchmark, or
runtime visual pass was run for this script-only release package.

## Findings

### 1. Readback and Upload Traffic Dominate the Measured Boot/Video Path

The 45 second Umineko Project startup/video telemetry pass recorded 1,903
readbacks totaling 36.35 GB and 6,255 texture uploads totaling 13.34 GB. The
average readback was about 19.1 MB, which is consistent with full-surface
downloads. In measured runs this has been a larger risk than shader fallback
pixel count.

Impact: high during video playback, text/glyph rendering, screenshot/save paths,
and any CPU fallback path that synchronizes full render targets.

Current status: the post-native-clipped-clear telemetry run recorded zero
readbacks and zero CPU shader fallback in the measured boot/video path. The
latest shutdown-lifetime verification run also kept CPU fallback at zero, but
recorded four 1920x1080 readbacks totaling 33.18 MB, apparently paired with
`generate_mipmaps`. The later breakup-path run recorded 3.90 GB of readbacks
from `alphaOutsideTextures.frag` CPU fallback; the source now embeds native
SPIR-V for the full built-in fragment shader inventory. The 2026-06-03
post-triangle-batching startup/video telemetry run again recorded zero readbacks
and zero CPU shader fallback in that path. The 2026-06-09 medium performance
pass changes mipmap texture recreation to copy the rendered base level
GPU-to-GPU before generating mipmaps, which should remove the paired
`generate_mipmaps` readback/reupload when the source texture is already current
on the GPU.

Next step: re-run source-tagged telemetry through the screenshot/downscale path
to verify `generate_mipmaps` no longer produces `ensure_pixels_current`
readbacks, then keep adding direct attribution for any remaining generic
readback sources.

### 2. Built-In Fragment Shader Bytecode Coverage Is Static-Complete

Known built-in fragment shaders now attempt native shadercross compilation when
the build has SDL_shadercross. Shaders that exceed the legacy GLSL translator's
coverage still fall back to CPU pixel loops. The common transition blend shader
uses embedded SPIR-V before shadercross, as do the blur and YUV video
conversion shaders. Runtime shaderc is no longer part of the default Windows
SDL3 public release link.

The measured Umineko Project boot/video path showed no CPU fallback for
`blendByMask.frag`, the blur path, or `colourConversion.frag`. The only CPU
shader fallbacks in that run were `colorModification.frag` and
`glyphGradient.frag`, totaling 704 draws and 579,731 pixels.

Impact: low in already measured boot/video paths, but runtime verification is
still required in scenes that exercise older breakup, subtitle atlas rendering,
pixelation, warp/whirl transitions, glass smash, text fade, and external
shader files.

Current status: verified. `glyphGradient.frag` and `colorModification.frag`
now compile once each as native shaders, draw 518 and 186 times respectively in
the measured path, and report zero CPU fallback draws/pixels. A later
breakup-path run found `alphaOutsideTextures.frag` as the next high-use CPU
fallback. The source now embeds native SPIR-V for every built-in fragment
shader under `Resources/Shaders`, and the 2026-06-03 UCRT64 build linked and
was copied to `D:\Umineko Project\onscripter-ru.exe`.

Next step: use representative playthrough telemetry to verify native draws for
the newly embedded shaders and to identify any external shader, render-to-self
safety case, unsupported texture format, or backend-specific native shader
failure that still falls back to the CPU.

### 3. Non-Blit Draws No Longer Submit Immediately

Current status: fixed and build-verified. Fixed-pipeline `GPU_TriangleBatch`
draws and native shader-program indexed draws now queue compatible submissions
through the native triangle batch. The remaining immediate non-blit path is the
native solid-rectangle helper used for clipped clears, which is not the same
small textured/shader draw stream that exposed this issue.

Impact: expected to be lower command-buffer pressure during shader-heavy
transitions and custom geometry paths. The synthetic benchmark confirms a large
reduction for compatible triangle batches; more effect-heavy telemetry is still
needed to quantify the impact in representative gameplay scenes.

Next step: use representative effect-heavy telemetry to compare command-buffer
count, native shader vertices, and `GPU_TriangleBatch` cost outside the
synthetic benchmark.

### 4. CPU Backing Storage Is Reduced But Still Necessary

Every `GPU_Image` keeps CPU backing pixels for the higher-level renderer
compatibility API, screenshot/readback support, CPU shader fallback, and
render-to-self recovery.
Lazy solid-color mirrors reduce the common full-clear cost, direct surface
copy-out avoids unnecessary persistent mirror updates, and reusable download
buffers reduce allocation churn. Actual CPU fallback still needs synchronized
pixels, and explicit readbacks still have to wait for GPU completion.

Impact: medium to high. Full-target readback at 1280x720 measured about
1.137 ms per direct GPU readback on this machine.

Next step: add explicit GPU-only image/target allocation for transient render
targets that are never consumed by CPU shader fallback, screenshot, or save
paths.

### 5. Startup Shader Work Is Mostly Classification

The SDL3 path compiles and links shader resources at renderer initialization,
but without shadercross this is mostly source classification and compatibility
program bookkeeping. It is not the dominant runtime cost compared with draw
submission and CPU shader execution.

Impact: low to medium for startup, low during steady-state gameplay.

### 6. SDL3_mixer Adapter Hot Paths Are Reduced But Need Listening Coverage

The SDL3_mixer compatibility layer creates tracks on channel allocation and
reuses them during playback. The notable runtime cost is quick-raw channels with
registered effects, which use an SDL_AudioStream callback and repeatedly feed
chunk data. That matches the old effect-hook behavior and is not the current
primary renderer performance risk.

Impact: low for normal music/SE playback, medium only for effect-heavy raw
stream channels.

Current status: dynamic BGM and mix-channel fades no longer quantize through the
integer SDL2_mixer volume API on SDL3 builds. They now preserve fractional
interpolated levels into SDL3_mixer's float track-gain path. Channel/music gain
state is preserved as float inside the SDL3 adapter, gain is reapplied after
track start, and explicit `dwave` channel reuse clears stale queued
mix-channel volume properties. A later shared SDL2/SDL3 fix also primes new
`dwave*` samples to volume 0 when the next script command is a timed
same-channel `ach_prop`, fixing the project-logo/Witch Hunt wind fade start
from stale engine SFX volume. The audio backend audit later removed the
duplicate decoded-buffer copy in `Mix_LoadWAV_RW()`, skips default playback
property allocation for non-looping starts, skips unchanged gain writes,
blocks instead of polling for non-event threaded sound loads, and avoids
copying already-mixer-format FFmpeg video-audio frames.

Next step: run a fade-heavy listening pass, including the project-logo/Witch
Hunt wind SFX, and keep monitoring for any unrelated fade stepping or restart
artifacts.

### 7. SDL3_GPU Shutdown Texture Lifetime

The telemetry runs emitted Vulkan validation errors during `vkDestroyDevice`
for unreleased `VkImage` and `VkImageView` children. That means at least some
SDL3_GPU textures/views were still alive when `SDL_DestroyGPUDevice()` ran.

Impact: low for frame time, medium for renderer correctness and default-cutover
confidence.

Current status: verified in the 2026-06-02 shutdown-lifetime telemetry run.
Live `GPU_Image` textures are tracked and released during `GPU_Quit()` before
the SDL_GPU device is destroyed, and the shutdown log no longer reports
unreleased `VkImage` or `VkImageView` children in this scenario.

Next step: keep monitoring validation output during longer in-game SDL3_GPU
passes, especially after exercising save/load, backlog, subtitles, video, and
resolution changes.

### 8. 144 Hz Frame Pacing and Glyph Cache Correctness

Current status: build-verified on 2026-06-18. `waitEvent()` now compensates
the pre-flip wait by a smoothed estimate of the post-wait frame tail. This
targets the observed 137-138 FPS ceiling on 144 Hz displays, where the old loop
waited a full refresh interval and then added flip/clear/title work on top.

The glyph cache key now keeps the real font preset id, initializes
`GlyphParams`, and uses matching hash/equality rules for colored versus
uncolored glyphs. Glyph measurements also normalize away color state. This
prevents preset menu text from reusing cached ruby-sized glyphs, which caused
some Grimoire letters to render smaller than their neighbors.

A follow-up 2026-06-18 pass fixed remaining Grimoire corruption after the first
cache-key change. The cache key could be correct while the shared FreeType
`Font` object was still left at the previous ruby-size face state when a cache
miss filled a glyph. `Fontinfo::my_font()` now reapplies the current style and
size before returning the font for kerning/layout, and glyph render/measure
cache misses reapply the key's style, size, and border immediately before
calling FreeType.

A second follow-up 2026-06-18 pass addressed Grimoire sections that still
showed lowercase-looking remnants in title entries. The glyph atlas is now
cleared when initialized, string-sprite render targets are explicitly cleared
before blended glyph draws, bold/italic face selection falls back to
`normal_face` instead of preserving a stale previous face, and copied colored
glyph values preserve their FreeType char index metadata for kerning.

A third follow-up 2026-06-18 pass fixed the remaining systemic Grimoire glyph
corruption risk introduced by the optimized batched text renderer. Prepared
glyph draw commands now record the glyph-cache generation, and a command batch
is rebuilt if the text atlas resets while commands are being prepared or before
the batch is rendered. This prevents queued draw commands from retaining
stale `GlyphValues` pointers after an atlas reset, which could show ruby-sized
or otherwise wrong glyph images in later Grimoire title sprites.

A fourth follow-up 2026-06-18 pass addressed the lower-level FreeType state
leak that could still create ruby-sized glyph cache entries with full-size
keys. `Font::setSize()` now validates the active `FT_Face`'s real pixel size
before skipping `FT_Set_Char_Size()`, so a face resized through another font
wrapper or style alias cannot leave later title rendering at the previous ruby
size.

Line-ending warnings were addressed by adding `.gitattributes` text/binary
classification and normalizing the currently modified text files to the local
Windows checkout convention. `git diff --check` is clean.

Verification: UCRT64 `make -j8` succeeded. SDL3 synthetic benchmark output was
written to `DerivedData/MinGW-x86_64/sdl3-benchmark-after-glyph-pacing-fix.csv`;
Music Box benchmark output was written to
`DerivedData/MinGW-x86_64/musicbox-benchmark-after-glyph-pacing-fix.txt`.
The follow-up font-state fix also passed UCRT64 `make -j8`; benchmark outputs
were written to `sdl3-benchmark-after-font-state-fix.csv` and
`musicbox-benchmark-after-font-state-fix.txt`. The second Grimoire cleanup
passed UCRT64 `make -j8`; benchmark outputs were written to
`sdl3-benchmark-after-grimoire-glyph-cleanup.csv` and
`musicbox-benchmark-after-grimoire-glyph-cleanup.txt`. The third Grimoire
generation fix passed UCRT64 `make -j8`; benchmark outputs were written to
`sdl3-benchmark-after-grimoire-generation-fix.csv` and
`musicbox-benchmark-after-grimoire-generation-fix.txt`. The FreeType face-size
fix passed UCRT64 `make -j8`; benchmark outputs were written to
`sdl3-benchmark-after-freetype-face-size-fix.csv` and
`musicbox-benchmark-after-freetype-face-size-fix.txt`.

Next step: perform an in-game visual pass through Config hover states and the
Grimoire list, then check the FPS overlay on the target 144 Hz display with the
rebuilt executable.

## Current Priority Order

1. Re-run representative shader telemetry at breakup-heavy, subtitle,
   pixelation, warp, whirl, old-breakup, glass-smash, and text-fade points to
   verify the full built-in SPIR-V set reports native draws with zero CPU
   fallback.
2. Run a fade-heavy audio listening pass to verify the SDL3_mixer high-resolution
   gain path, channel restart cleanup, and shared `dwave`/`ach_prop` fade
   priming behavior.
3. Verify the GPU-to-GPU mipmap recreation path removes the previous
   `generate_mipmaps`/`ensure_pixels_current` 1920x1080 readback pair.
4. Separate GPU-only transient images from readback-capable images.
5. Run broader visual/audio regression across save/load UI, backlog, subtitles,
   videos, transitions, fullscreen/resolution changes, audio fades/loops, and
   device-change/pause-resume cases.
6. Use renderer telemetry from representative playthroughs to identify any
   external shader, render-to-self safety case, unsupported texture format, or
   backend-specific native shader failure that still falls back to CPU.
7. Use representative effect-heavy telemetry to quantify native indexed
   triangle/shader draw batching outside the synthetic benchmark.
8. Add an in-game scripted benchmark scene once a deterministic script path is
   available for Umineko Project release data.
9. Continue monitoring Vulkan validation output in longer runs while comparing
   command buffers, texture uploads, readbacks, CPU shader pixels, and rendered
   native vertices before and after later renderer optimizations.
