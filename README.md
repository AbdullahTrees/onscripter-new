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
