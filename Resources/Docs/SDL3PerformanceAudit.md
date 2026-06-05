# SDL3 Performance Audit

Date: 2026-06-02
Updated: 2026-06-05

This audit covers the SDL3 default renderer path, with emphasis on
`Engine/Graphics/SDL3GPUCompat.cpp` because that layer currently adapts the
engine's former SDL2_gpu-shaped renderer API to SDL3_GPU.

## Benchmark

The SDL3 build now has an opt-in benchmark mode that exits before game script
initialization:

```sh
./onscripter-ru.exe --sdl3-benchmark
```

Optional arguments:

```sh
./onscripter-ru.exe --sdl3-benchmark \
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
menu also has a `Restart` button immediately to the left of `Back`, with the
same horizontal spacing used between `Next` and `Exit`, so restart-required
settings no longer leave users with only a passive message.

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

### Renderer Telemetry

The SDL3_GPU backend now has opt-in shutdown telemetry. Enable it with:

```sh
./onscripter-ru.exe --sdl3-gpu-telemetry
```

or:

```sh
ONS_SDL3_GPU_TELEMETRY=1 ./onscripter-ru.exe
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
and zero CPU shader fallback in that path.

Next step: add direct source attribution for future `ensure_pixels_current`
readbacks, starting with the `generate_mipmaps` path, then re-run representative
shader telemetry points to verify the newly embedded built-in SPIR-V paths no
longer create CPU fallback readbacks.

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

## Current Priority Order

1. Re-run representative shader telemetry at breakup-heavy, subtitle,
   pixelation, warp, whirl, old-breakup, glass-smash, and text-fade points to
   verify the full built-in SPIR-V set reports native draws with zero CPU
   fallback.
2. Run a fade-heavy audio listening pass to verify the SDL3_mixer high-resolution
   gain path, channel restart cleanup, and shared `dwave`/`ach_prop` fade
   priming behavior.
3. Attribute the latest `generate_mipmaps`/`ensure_pixels_current` 1920x1080
   readback pair so future readbacks are not left in a generic bucket.
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
