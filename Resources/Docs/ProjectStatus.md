# Project status

This file records the maintained build and platform state. Release-by-release
history belongs in Git, issues, and release notes rather than this document.

## Supported release targets

- Windows 10 or newer, x86-64, built in MSYS2 UCRT64.
- Android 14 or newer, arm64.
- The SDL3/SDL3_GPU renderer is the only supported renderer.

The obsolete SDL2-based Xcode project has been removed so it can no longer
produce misleading builds. The configure-based macOS path is best-effort until
a current SDL3 build is exercised in CI.

## Windows prerequisites

Install MSYS2, open its UCRT64 shell, and install:

```sh
pacman -S --needed base-devel \
  mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-meson \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-nasm \
  mingw-w64-ucrt-x86_64-pkgconf
```

Then build from the repository root:

```sh
./configure --release-build --strip-binary --std=gnu++23
make -j8
```

`configure` downloads source archives for the pinned dependency graph, verifies
their SHA-256 hashes, and builds static libraries below `DerivedData`.

## Other host builds

A C++23 compiler, GNU Make, CMake, Meson, Ninja, NASM, `pkg-config`, and normal
POSIX development tools are required:

```sh
./configure --release-build --std=gnu++23
make -j8
```

Cross-compilation options are listed by `./configure --help`. Platform support
should only be claimed after a clean build and runtime smoke test on that target.

## Dependency policy

Active dependencies are pinned in `Dependencies/pkgs` with cryptographic hashes.
The aggregate package stamp depends on every active recipe and patch, so a
recipe change forces version evaluation instead of silently retaining an old
static library. Patch releases should be reviewed monthly and security fixes
expedited. Major upgrades of FFmpeg, Lua, or SDL still require game-data and
media regression testing. The current audited lines are FFmpeg 8.1, Lua 5.5,
and SDL3; retired SDL2 recipes were removed.

## Verification

The repository has Windows build/test CI and standalone tests for archive
indexes, command-line validation, serialized state, regular expressions, and
multidimensional variables. Linux CI runs those tests under ASan/UBSan and runs
four libFuzzer targets on pushes and a larger weekly budget. Static analysis
remains part of the release audit.

The synthetic compressed-script fixture verifies public-release startup and
orderly shutdown without copyrighted assets. It is not a representative game
regression suite. A release is not fully verified until it has:

1. passed a clean Windows UCRT64 build;
2. passed compiler warnings and static analysis;
3. passed the native tests and sanitizer/fuzz CI;
4. started with legal Umineko Project data;
5. exercised saves, text, audio, video, menus, and archive loading; and
6. completed the built-in SDL3 benchmark on representative hardware.

That missing deterministic game corpus is the main limit on aggressive renderer
or ownership refactors and on claims of maximal optimization.
