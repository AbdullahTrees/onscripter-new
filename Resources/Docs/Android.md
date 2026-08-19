# Android

Reference for the Android target: how the native and Java halves fit together,
the contracts between them, and the environment the build assumes.

This documents current architecture, not change history — for what changed and
when, read the git log.

## Contents

- [Building and testing with Android Studio](#building-and-testing-with-android-studio)
  - [Prerequisites](#prerequisites)
  - [Step 1 — obtain an engine binary](#step-1--obtain-an-engine-binary)
  - [Step 2 — open and run](#step-2--open-and-run)
  - [Step 3 — supply game data](#step-3--supply-game-data)
  - [Iteration loop](#iteration-loop)
  - [Troubleshooting](#troubleshooting)
- [Build pipeline](#build-pipeline)
  - [Why the first build is slow](#why-the-first-build-is-slow)
- [Build environment](#build-environment)
  - [Line endings](#line-endings)
  - [NDK discovery](#ndk-discovery)
- [Supported target](#supported-target)
- [Native architecture](#native-architecture)
- [Java to native contract](#java-to-native-contract)
  - [The Java layer is not a launcher](#the-java-layer-is-not-a-launcher)
  - [SDL version lock](#sdl-version-lock)
  - [How complete the SDL3 port is](#how-complete-the-sdl3-port-is)
  - [Why paths are passed as arguments, not environment variables](#why-paths-are-passed-as-arguments-not-environment-variables)
- [Storage model](#storage-model)
- [Android-specific code](#android-specific-code)
  - [Tier 1 — wholly Android-only](#tier-1--wholly-android-only)
  - [Tier 2 — shared files with Android-only regions](#tier-2--shared-files-with-android-only-regions)
  - [Tier 3 — do not touch for Android work](#tier-3--do-not-touch-for-android-work)
- [Debugging](#debugging)
  - [Log tags](#log-tags)
  - [Always force-stop between launches](#always-force-stop-between-launches)
  - [Test a Java-only change without a native rebuild](#test-a-java-only-change-without-a-native-rebuild)
  - [Test a link-flag change without relinking](#test-a-link-flag-change-without-relinking)
- [Known gaps](#known-gaps)

## Building and testing with Android Studio

`Resources/Droid` is a complete Gradle project — own `settings.gradle`, wrapper
and namespace — but it is **not hermetic and not buildable from a bare clone**.
It reaches outside itself into `../../DerivedData` for the engine binary, and
that binary is never committed. A fresh clone therefore has no `libmain.so` and
`syncEngineLibs` fails the build until step 1 is done. That failure is
deliberate: warning instead would produce an APK with no native library, which
installs and then crashes on launch.

### Prerequisites

| Component | Needed for |
| --- | --- |
| Android Studio, SDK platform 36, build-tools 36 | packaging, install, run, debug |
| JDK 17+ | Android Studio's bundled JBR is fine |
| NDK `29.0.14206865` | **only** to compile the engine |
| MSYS2 (Windows) or a POSIX shell | the `configure`/`make` engine build |

Note the split: **Gradle never compiles the engine.** If a `libmain.so` already
exists you can build, install and run the APK with no NDK at all — AGP wants it
only to strip symbols, and that degrades to a warning:

```
Unable to strip the following libraries, packaging them as they are: libmain.so
```

The APK is simply packaged unstripped, which is fine for development.

### Step 1 — obtain an engine binary

`syncEngineLibs` in `build.gradle` copies `DerivedData/Droid-<arch>/onscripter-new`
into `lib/<abi>/libmain.so` before every Gradle build. There are two ways to
produce that file.

**Path A — build from source.** Authoritative, and required for any C++ or
link-flag change.

```sh
export ANDROID_SDK_ROOT="$HOME/AppData/Local/Android/Sdk"   # Windows
./configure --droid-build --droid-arch=arm64
make -j$(nproc)
```

The first run compiles 17 dependencies from source — see *Why the first build is
slow*. Later runs reuse the `.pkgs` stamps and only recompile engine code.

**Path B — reuse a released binary.** Fast, and sufficient for all Java-side
work.

Because everything is statically linked into `libmain.so` (see *Native
architecture*), a binary from any release is self-contained and can be dropped
straight into `DerivedData`:

```sh
# from a connected device, or just unzip a downloaded release APK
adb pull "$(adb shell pm path org.umineko_project.onscripter_ru | sed 's/package://')" base.apk
unzip -q base.apk -d apkx
mkdir -p DerivedData/Droid-aarch64
cp apkx/lib/arm64-v8a/libmain.so DerivedData/Droid-aarch64/onscripter-new
```

This skips hours of dependency compilation. Understand what it is not: the
binary embeds whatever engine sources that release was cut from, so it cannot
validate C++ or link changes, and the next `make` overwrites it. If the release
predates a link fix you need, patch its `DT_NEEDED` first — see *Test a
link-flag change without relinking*.

Engine binaries are deliberately **not** committed to the repository. They are
12 MB each, change on every build, would bloat history permanently, and — worst
— a stale checked-in binary silently masks source changes. Path B gets the same
speed from an artifact that is already published and versioned.

### Step 2 — open and run

Open **`Resources/Droid`** as the project. Not the repository root; that is not
a Gradle project.

Android Studio writes `local.properties` on first sync, or create it manually:

```
sdk.dir=C:/Users/<you>/AppData/Local/Android/Sdk
```

Then select a device and press **Run**. The command-line equivalent is:

```sh
cd Resources/Droid
./gradlew assembleDebug
adb install -r build/outputs/apk/debug/onscripter-new-debug.apk
```

### Step 3 — supply game data

The engine exits immediately without it. Push a legally obtained Umineko
Project installation to the app-scoped directory:

```sh
MSYS_NO_PATHCONV=1 adb push <game-dir>/. \
  /sdcard/Android/data/org.umineko_project.onscripter_ru/files/ONScripter-RU/
```

`MSYS_NO_PATHCONV=1` is required on Windows or the destination is rewritten as
a Windows path. Large data sets transfer faster over MTP.

### Iteration loop

**Gradle never compiles the engine.** There is no `externalNativeBuild` in the
project and no C++ task of any kind, so pressing Run after editing engine
sources would otherwise package the previous binary and silently omit the
change. `checkEngineFreshness` guards against that: it compares the newest file
under `Engine/`, `Support/` and `External/` against the staged library and fails
the build if sources are newer.

- **Java change** — press Run. No native rebuild needed.
- **C++ change** — re-run `make` in a terminal *first*, then press Run.
  `syncEngineLibs` notices the newer binary and restages it.
- **Only touched a C++ file incidentally** — pass `-PallowStaleEngine` (or add it
  to the run configuration) to package the existing binary anyway.
- **Native breakpoints** — set the run configuration's debugger to **Dual**.
  Symbols come from the unstripped binary in `DerivedData`.

`Scripts/apkbuild.tool` remains the path for reproducible command-line and
release packaging; it stages a copy of this same Gradle project under
`DerivedData/Droid-package`.

### Troubleshooting

| Symptom | Cause and fix |
| --- | --- |
| Build fails in `:syncEngineLibs` with `No engine binary found` | No engine binary yet. Do step 1. |
| Build fails in `:checkEngineFreshness` with `Engine sources are newer` | A C++ change has not been compiled. Run `make`, or `-PallowStaleEngine` to ignore. |
| `INSTALL_FAILED_UPDATE_INCOMPATIBLE` | An existing install was signed with a different key. `adb uninstall org.umineko_project.onscripter_ru` first. |
| `Unable to strip the following libraries` | Benign. AGP has no NDK; the APK is packaged unstripped. |
| `Invalid launch directory!` then exit | No game data at the scoped path. Do step 3. |
| `UnsatisfiedLinkError` on a `native` method | The library failed to load entirely. Read the `dlopen failed:` line from `nativeloader`, not the stack trace. |
| App resumes instead of restarting | Force-stop first; the engine aborts on a reused pid. |

## Build pipeline

```
Scripts/ndktoolchain.sh    → wrapper toolchains under DerivedData/ndk/toolchain-<arch>
./configure --droid-build --droid-arch=arm64
make                       → DerivedData/Droid-aarch64/onscripter-new
make apk                   → Scripts/apkbuild.tool → Gradle → onscripter-new.apk
```

`Scripts/quickdroid.tool [--release|--debug]` runs the whole ABI matrix.

### Why the first build is slow

`make` builds **17 dependencies from source** before it touches engine code, once
per ABI. The bottleneck is not compilation — that is parallel at `-j$(nproc)` —
but the autotools `configure` scripts, which are strictly serial and spawn one
compiler process per feature probe (FFmpeg's runs on the order of a thousand).
MSYS emulates `fork()`, making process creation 10–50x more expensive than on
Linux, so the serial probe phase dominates wall clock. The project's own CI
budgets 90 minutes for a single Windows target.

Completed packages are stamped in `DerivedData/onscrlib/.pkgs/<name>` with
`pkgver-pkgrel` and skipped on later runs, so the cost is paid once. Building
only `arm64` roughly halves it; `x86_64` is emulator-only.

No prebuilt dependency bundles are published — `onscrlib` is only a meta-package
listing dependencies, and releases ship just the APK and a Windows zip. The
dependency build can still be skipped entirely for Java-side work by reusing the
already-linked `libmain.so` from a release APK; see *Step 1 — obtain an engine
binary*.

## Build environment

### Line endings

`.gitattributes` pins `configure`, `gradlew`, `*.sh`, `*.tool` and `*.pkgbuild`
to `eol=lf`. Without this, a Windows clone with `core.autocrlf=true` produces
CRLF shebangs that MSYS bash refuses (`bad interpreter: /bin/bash^M`). A clone
predating those rules needs `git add --renormalize .` once.

### NDK discovery

`Scripts/ndktoolchain.sh` looks for NDK `29.0.14206865` in `ANDROID_NDK_HOME`,
`ANDROID_NDK_ROOT`, `$ANDROID_SDK_ROOT/ndk/`, `$ANDROID_HOME/ndk/`, then the
default SDK locations for Windows, macOS and Linux. If none match it downloads
its own copy into `DerivedData/ndk`. Setting `ANDROID_SDK_ROOT` avoids a
redundant multi-gigabyte download when Android Studio already has the NDK.

It generates thin wrapper scripts (`clang`, `clang++`, plus `.cmd` variants on
Windows) pinning `--target=<abi><api>`, rather than using the removed
`make_standalone_toolchain.py`.

## Supported target

Defined in `Resources/Droid/build.gradle` and `Scripts/ndktoolchain.sh`:

| | |
| --- | --- |
| minSdk / targetSdk / compileSdk | 34 / 36 / 36 |
| ABIs | `arm64-v8a`, `x86_64` |
| NDK | r29 (`29.0.14206865`) |
| Java | 17 |
| AGP | 9.2.0, Gradle 9.4.1 |
| Renderer | SDL3 `SDL_GPU` (Vulkan) — no GLES fallback exists |

`armeabi-v7a` and `x86` are not supported.

## Native architecture

**Everything is statically linked into one `libmain.so` per ABI.** SDL3,
SDL3_image, SDL3_mixer, FFmpeg, harfbuzz, freetype, libass and the rest are `.a`
archives absorbed at link time. The APK contains exactly two native files:

```
lib/arm64-v8a/libmain.so
lib/x86_64/libmain.so
```

Three consequences that are easy to trip over:

- **There is no `libSDL3.so`.** Anything that tries to `dlopen` SDL by name
  fails. `SDLActivity.getLibraries()` defaults to `{"SDL3", "main"}` and must be
  overridden.
- **The engine's entry point is plain `main`.** `Engine/Core/Loader.cpp` declares
  `int main(int, char **)` and no file in the tree includes `SDL_main.h`, so
  SDL2's `#define main SDL_main` shim is not in effect. `libmain.so` exports
  `main`; `SDL_main` does not exist.
- **System libraries must be named explicitly at link time.** SDL3's pkg-config
  output supplies `libandroid`, `liblog`, `libGLESv2` and `libOpenSLES`, but
  nothing propagates FFmpeg's MediaCodec dependency. Without `-lmediandk` the
  binary carries roughly twenty undefined `AMediaCodec_*` / `AMediaFormat_*`
  symbols and `dlopen` fails outright. It is set in the `*clang*:"Droid")` branch
  of `configure`.

Verify the link surface of any build with `readelf -dW libmain.so | grep NEEDED`
and by listing undefined dynamic symbols.

## Java to native contract

`ONSActivity` exists to reconcile stock `SDLActivity` with the facts above. Its
overrides are load-bearing; removing any of them breaks startup.

| Override | Returns | Why |
| --- | --- | --- |
| `getLibraries()` | `{"main"}` | No `libSDL3.so` exists |
| `getMainFunction()` | `"main"` | Engine exports `main`, not `SDL_main` |
| `getArguments()` | `{"--root", <scoped path>}` | Points the engine at app-scoped storage |

### The Java layer is not a launcher

It is tempting to read `SDLActivity` as a thin shim that opens a native
application. It is not. Android exposes no way for native code to obtain a
window, input events or an audio device on its own, so the Java side owns all of
it and bridges back over JNI. Current size:

| File | Lines | Native methods |
| --- | --- | --- |
| `SDLActivity.java` | 2240 | 56 |
| `SDLControllerManager.java` | 1010 | 10 |
| `HIDDeviceBLESteamController.java` | 829 | 1 |
| `HIDDeviceManager.java` | 698 | 8 |
| `SDLSurface.java` | 464 | 0 |
| `HIDDeviceUSB.java` | 354 | 0 |
| `SDLInputConnection.java` | 135 | 2 |
| `SDLAudioManager.java` | 126 | 3 |
| `SDL.java` | 90 | 2 |
| `ONSActivity.java` | 81 | 1 |
| `SDLDummyEdit.java` | 65 | 0 |
| `SDLSensorManager.java` | 31 | 0 |
| `HIDDevice.java` | 21 | 0 |

That is ~6100 lines and 82 native entry points covering the rendering surface,
touch/key/mouse/gamepad input, sensors, IME and soft keyboard, audio device
lifecycle, USB and Bluetooth HID, clipboard, permissions, and translation of the
activity lifecycle into SDL events. Treat it as a port layer, not glue.

Only `ONSActivity.java` is project code. The rest are vendored SDL3 sources and
should be replaced wholesale on an SDL upgrade rather than edited.

### SDL version lock

There is no separate SDL3 runtime — it is compiled into `libmain.so` — and the
vendored Java sources are pinned to the exact version they came from.
`SDLActivity.java` hardcodes the expected version and verifies it at startup:

```java
private static final int SDL_MAJOR_VERSION = 3;
private static final int SDL_MINOR_VERSION = 4;
private static final int SDL_MICRO_VERSION = 10;
...
String version = nativeGetVersion();
if (!version.equals(expected_version))
    errorMsgBrokenLib = "SDL C/Java version mismatch (expected ..., got ...)";
```

Those constants must match `pkgver` in `Dependencies/pkgs/SDL3.pkgbuild`
(currently `3.4.10`). Bumping SDL3 therefore means re-vendoring the Java sources
from the matching SDL release and updating these constants, or the app refuses
to start with a mismatch dialog.

### How complete the SDL3 port is

The renderer is a genuine full port: the third-party SDL_gpu dependency was
removed and replaced with SDL3's native `SDL_GPU` API plus precompiled
SPIR-V/DXIL/MSL shaders under `Engine/Graphics/SDL3GPUShaders/`.

Elsewhere, SDL2 assumptions survive in places and are worth suspecting first when
Android startup misbehaves. Two known examples, both of which prevented launch:
the engine declares a plain `main` with no `SDL_main.h` (SDL2 supplied the
`SDL_main` alias via macro), and the storage code assumed `nativeSetenv` writes
to libc's `environ` (see below).

### Why paths are passed as arguments, not environment variables

Under SDL2, `SDLActivity.nativeSetenv` wrapped POSIX `setenv()`. Under **SDL3 it
writes to SDL's own environment object**, which is disconnected from libc's
`environ`. `Support/FileIO.cpp` resolves the launch directory with
`std::getenv("EXTERNAL_STORAGE")`, so it never observes anything set that way and
falls back to Android's default `/sdcard`. SDL also logs
`Request to get environment variables before JNI is ready`.

`--root` is therefore the reliable channel; the engine treats it as authoritative
and refuses to let a later path override it. A cleaner long-term fix is to call
`SDL_GetAndroidExternalStoragePath()` directly in the `DROID` branch of
`FileIO.cpp` and drop the environment dependency, which needs a native rebuild.

Related: **Android's `System.loadLibrary` uses `RTLD_NOW` without `RTLD_GLOBAL`.**
Preloading a system library from Java does not make its symbols visible to
libraries loaded afterwards, so a missing dependency cannot be papered over that
way — it must be recorded in `libmain.so`'s own `DT_NEEDED`.

## Storage model

No `WRITE_EXTERNAL_STORAGE`, no SAF import flow. `ONSActivity` resolves
`getExternalFilesDir(null)`, appends `ONScripter-RU`, creates it, and passes it
as `--root`. Game data belongs in:

```
/sdcard/Android/data/org.umineko_project.onscripter_ru/files/ONScripter-RU/
```

On Android 13+ that path is unreachable from most on-device file managers; use
`adb push` or MTP. Saves live in a `SaveData` subdirectory of the launch dir.

## Android-specific code

Android work is not confined to `Resources/Droid`. It spans that directory, the
`Support/Droid` sources, `#if defined(DROID)` regions inside otherwise shared
files, and the Droid branch of `configure` — link flags in particular live there,
not in the Gradle project.

Work on this target should stay inside tiers 1 and 2.

### Tier 1 — wholly Android-only

```
Resources/Droid/**        manifest, build.gradle, gradle wrapper, res/, 13 Java sources
Support/Droid/**          DroidProfile.cpp / .hpp
Scripts/ndktoolchain.sh   NDK discovery and wrapper toolchain generation
Scripts/apkbuild.tool     Gradle/AGP packaging
Scripts/quickdroid.tool   multi-ABI build driver
```

### Tier 2 — shared files with Android-only regions

Edit only inside `#if defined(DROID)` guards (~42 sites across 14 files):

```
Engine/Media/HardwareDecoder.cpp     MediaCodec hwaccel, JNI vm registration
Engine/Media/VideoDecoder.cpp, Controller.hpp
Engine/Graphics/GPU.cpp, GPU.hpp
Engine/Core/ONScripter.cpp, Command.cpp, CommandExt.cpp
Engine/Core/Event.cpp, Loader.cpp, Animation.cpp
Engine/Components/Window.hpp
Support/FileIO.cpp                   storage paths, __android_log logging
External/Compatibility.hpp
```

Build files with Android-only regions: the `*clang*:"Droid")` branch of
`configure`, and `configopts_droid` / `cflags_droid` blocks in
`Dependencies/pkgs/*.pkgbuild`.

### Tier 3 — do not touch for Android work

Everything else, including the rest of `Engine/`, `Engine/Graphics/SDL3GPU*`,
`Tests/`, `Resources/Windows/` and `Support/Apple/`.

## Debugging

### Log tags

| Tag | Source |
| --- | --- |
| `ONScripter-RU` | engine, via `FileIO::log` and `__android_log_vprint` |
| `ONSActivity` | the app's Activity subclass |
| `SDL` | SDL3's Java and native layers |
| `nativeloader`, `System.err` | dynamic linker — **where real `dlopen` failures appear** |

A Java `UnsatisfiedLinkError` on a `native` method usually means the whole
library failed to load, not that the method is missing. The named method is
simply the first JNI call attempted. Always read the `dlopen failed:` line above
it rather than trusting the stack trace.

### Always force-stop between launches

`Engine/Core/Loader.cpp` compares the current pid against a static `previousPid`
and aborts if they match, because a reused process would run with stale state and
an already-loaded library. Relaunching without a force-stop resumes the old
process and produces confusing logs.

```sh
adb shell am force-stop org.umineko_project.onscripter_ru
adb logcat -c
adb shell am start -n org.umineko_project.onscripter_ru/.ONSActivity
```

`adb push` to `/sdcard/...` from MSYS needs `MSYS_NO_PATHCONV=1`, otherwise the
destination is rewritten as a Windows path.

### Test a Java-only change without a native rebuild

The native build takes hours; changes confined to `Resources/Droid/src` do not
need it. Reuse the existing `libmain.so` and swap only the dex: compile the Java
sources with `javac --release 17` against the platform `android.jar`, dex them
with `d8 --min-api 34`, replace `classes.dex` inside a copy of the APK, then
`zipalign -f -p 4` and re-sign with `apksigner`. On Windows those build-tools
binaries need Windows-style paths — convert with `cygpath -w`. The re-signed APK
will not match the release signature, so uninstall the old one first.

### Test a link-flag change without relinking

A missing `DT_NEEDED` can be injected into an existing `.so` to confirm a fix
before committing to a multi-hour rebuild:

```python
import lief                      # pip install lief
b = lief.ELF.parse("libmain.so")
b.add_library("libmediandk.so")
b.write("libmain-patched.so")
```

Repack the patched `.so` into the APK using the dex procedure above.

## Known gaps

- CI (`.github/workflows/build.yml`) covers Windows and Linux only. There is no
  Android build or smoke test, so Android regressions are caught by hand.
- No deterministic game-data regression corpus exists, which limits confidence in
  renderer and media changes. `Tests/Fixtures/SmokeGame/0.txt` is a minimal
  script, not a runnable game — the engine still reports
  `Invalid launch directory!` with only that present.
- `android:screenOrientation="sensorLandscape"` is ignored on targetSdk 36;
  Android 16 drops manifest-declared fixed orientation on large screens.
