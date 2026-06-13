# onscripter-new

`onscripter-new` is the current Umineko Project modernization branch of
ONScripter-RU. It keeps the project-specific visual novel runtime and script
compatibility surface while moving the active build, dependency, renderer, and
audio stack onto current toolchains.

This repository is not a general-purpose visual novel SDK. It is shaped around
Umineko Project release data, compatibility requirements, save paths, and
runtime behavior.

## Current Status

- SDL3 is the active renderer stack; the old SDL2_gpu/libepoxy and legacy
  GL/GLES renderer paths have been removed from the current configure flow.
- Windows public builds target x86_64 MSYS2/UCRT64 with a Windows 10 runtime
  floor.
- Windows release builds statically link SDL3, SDL3_image, SDL3_mixer, and the
  project dependency stack through `onscrlib`.
- Android packaging targets Android 14/API 34 minimum and Android 16/API 36,
  with `arm64-v8a` and `x86_64` native libraries.
- Runtime-facing branding, window titles, package labels, and release artifact
  names now use `onscripter-new`.
- The consolidated maintenance document is
  [Resources/Docs/ProjectStatus.md](Resources/Docs/ProjectStatus.md).

## Differences from ONScripter-RU

`onscripter-new` is derived from ONScripter-RU, but it is no longer maintained
as a generic upstream-compatible engine drop-in. The main differences are:

| Area | ONScripter-RU | onscripter-new |
| --- | --- | --- |
| Project scope | General ONScripter-family runtime with broad legacy script and platform coverage. | Umineko Project-focused runtime, release packaging, defaults, and documentation. |
| Branding | Uses `onscripter-ru` executable, app, and package names. | Uses `onscripter-new` executable names, window/app labels, Android package labels, and release artifact names. |
| Renderer stack | Historically carried SDL2_gpu, libepoxy, and legacy GL/GLES paths. | Uses SDL3_GPU through the current configure flow; removed the old SDL2_gpu/libepoxy and legacy GL/GLES renderer paths from active builds. |
| Media path | More legacy renderer/media compatibility code remains upstream. | Adds SDL3-oriented GPU upload/readback cleanup, embedded native shaders, and direct GPU-assisted video/color conversion work for the Umineko Project workload. |
| Build targets | Supports older platform/toolchain combinations inherited from ONScripter-RU. | Targets current maintained floors: Windows 10 x86_64, modern Linux, macOS/iOS baselines, and Android 14/API 34 or newer. |
| Windows builds | Often depends on the local runtime/dependency layout chosen by the builder. | Public Windows releases are MSYS2/UCRT64 x86_64 builds with the SDL3 dependency stack statically linked through `onscrlib`. |
| Android builds | Uses ONScripter-RU Android naming and older target assumptions. | Packages `onscripter-new` APKs with current Android SDK/NDK targets and `arm64-v8a` plus `x86_64` native libraries. |
| Release data | Does not ship this branch's Umineko Project-specific packed English script. | Windows releases include `onscripter-new.exe`, the current packed `en.file`, install notes, and SHA-256 checksums. |
| Runtime UX | Upstream behavior and menus are kept closer to the original engine/project state. | Carries Umineko Project-specific UI/script maintenance, including file verification copy, pause-menu fixes, Message Browser line-jump confirmation, and modernized control text. |

Compatibility is intentionally practical rather than universal: this branch
preserves the ONScripter-RU behavior needed by compatible Umineko Project
release data, while active maintenance favors the current Umineko Project
desktop and Android packages.

## AI Disclosure

AI models were used during this modernization effort for code review, cleanup,
documentation updates, build verification support, and release preparation.
Human review and local build/package verification were still required before
publishing release artifacts.

## Installation

Release artifacts do not include copyrighted Umineko Project game data. You need
an existing legal Umineko Project installation or compatible release data.

### Windows

1. Download `onscripter-new-windows-x86_64.zip` from the latest GitHub release.
2. Extract the archive.
3. Copy `onscripter-new.exe` and `en.file` into the Umineko Project game
   directory, replacing the existing engine executable and packed English
   script as needed.
4. Launch `onscripter-new.exe` from the game directory.

The Windows package is intentionally small because the release executable is
statically linked against the active SDL3 dependency stack.

### Android

1. Download `onscripter-new-android.apk` from the latest GitHub release.
2. Install the APK on an Android 14/API 34 or newer device.
3. Copy compatible Umineko Project game data to the app data location expected
   by the Android build.
4. Launch the installed app.

The APK contains the Android engine package, not the full game data.

### Integrity Checks

Each release includes `SHA256SUMS.txt`. After downloading artifacts, compare
their SHA-256 hashes with the checksum file before installing.

## Building

For a normal host build:

```sh
./configure
make -j8
```

For the current Windows public-release build used with packaged Umineko Project
data:

```sh
./configure --release-build --strip-binary --std=gnu++23
make -j8
```

Development builds expect plaintext script layouts. Packaged game distributions
that ship compressed `.file` scripts should use a public release build.

See [Resources/Docs/ProjectStatus.md](Resources/Docs/ProjectStatus.md) for
platform floors, dependency notes, Android packaging commands, verification
history, and SDL3 performance audit details.

## Credits

- Ogapee
- "Uncle" Mion Sonozaki
- Umineko Project
- ONScripter-RU contributors
- Third-party library authors
