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

Project maintenance docs live in `D:\onscripter-new\Resources\Docs`. Before
code changes, check this file plus `DependencyAudit.md` and
`SDL3PerformanceAudit.md`.

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

SDL3_GPU telemetry can be enabled at runtime with `--sdl3-gpu-telemetry` or
`ONS_SDL3_GPU_TELEMETRY=1`. The renderer logs aggregate command-buffer,
texture-upload, readback, native-draw, CPU-blit, CPU-shader-fallback, and
per-shader native/fallback counters on exit. Texture upload and readback
traffic is also split into per-source buckets such as video frame uploads,
surface copy-out, glyph atlas `simulateRead()`, and CPU fallback paths. The
latest source-tagged Umineko Project runs are summarized in
`Resources/Docs/SDL3PerformanceAudit.md`; after the embedded glyph/color
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
./onscripter-ru.exe --sdl3-benchmark
./onscripter-ru.exe --sdl3-benchmark --sdl3-benchmark-iterations 300 --sdl3-benchmark-width 1280 --sdl3-benchmark-height 720
./onscripter-ru.exe --sdl3-benchmark --sdl3-benchmark-output sdl3-benchmark.csv
```

See `Resources/Docs/SDL3PerformanceAudit.md` for benchmark cases and current
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
- No line numbers ("Editor" → "Appearance" → "Enable line numbers")
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
- `--normal` — normal developer build (default)
- `--release` — stripped release build
- `--debug` — debug build

**Debugging the binaries**:

It is recommended to debug using IDA Pro.

1. Setting Java debugger in order to properly start the application. It is worth checking the [official documentation](https://www.hex-rays.com/products/ida/support/tutorials/debugging_dalvik.pdf) first.
   
    1. Open classes.dex in (32-bit) IDA Pro by dragging onscripter-new.apk into its main window
    2. Put a breakpoint on `_def_Activity__init_@V`
    3. Go to `Debugger` → `Debugger options` → Set specific options and fill adb path
    4. Launch the debugger and specify source path mapping (`.` → `path/to/onscripter/sources`)

2. Setting hardware debugger in order to debug the binary.

    1. Open `libmain.so` in IDA Pro by dragging `onscripter-new.apk` into its main window
    2. Set debugger to `Remote Linux Debugger`
    3. Upload a correct android debugger server to the device (e.g. to `/data/debug/`):
        - `android_server64` — for arm64
        - `android_x86_64_server` — for x86_64

        You may use the following command:
        ```
        adb push android_server64 /data/debug/
        ```

    4. Set debugger executable permissions to 0777 and run the debugger (use adb shell).
    5. Set `Debugger` → `Process` options parameters:
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

_Unlike macOS (manual architecture specification) and Windows (untested 64-bit binary), the executable architecture on Linux depends on the default compiler architecture. On 32-bit systems 32-bit binaries are normally produced and on 64-bit systems — 64-bit binaries._

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
