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
  - [Why SAF alone cannot work](#why-saf-alone-cannot-work)
  - [The flow](#the-flow)
  - [What the picker refuses](#what-the-picker-refuses)
  - [Fallback and recovery](#fallback-and-recovery)
  - [Where saves go](#where-saves-go)
  - [Diagnosing access from the shell](#diagnosing-access-from-the-shell)
- [Android-specific code](#android-specific-code)
  - [Tier 1 — wholly Android-only](#tier-1--wholly-android-only)
  - [Tier 2 — shared files with Android-only regions](#tier-2--shared-files-with-android-only-regions)
  - [Tier 3 — do not touch for Android work](#tier-3--do-not-touch-for-android-work)
- [Debugging](#debugging)
  - [Log tags](#log-tags)
  - [Engine logging on Android](#engine-logging-on-android)
  - [Always force-stop between launches](#always-force-stop-between-launches)
  - [Test a Java-only change without a native rebuild](#test-a-java-only-change-without-a-native-rebuild)
  - [Test a link-flag change without relinking](#test-a-link-flag-change-without-relinking)
- [Backlog](#backlog)
  - [Defects](#defects)
  - [Features](#features)
  - [Device compatibility](#device-compatibility)
  - [Infrastructure](#infrastructure)
  - [Unverified](#unverified)

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
| MSYS2 UCRT64 (Windows) or a POSIX shell | the `configure`/`make` engine build |

On Windows the engine build must run in the MSYS2 UCRT64 environment. A base
MSYS2 install is not enough — the dependency recipes need its toolchain:

```sh
pacman -S --needed base-devel \
  mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-meson \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-nasm \
  mingw-w64-ucrt-x86_64-pkgconf
```

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

The engine exits immediately without it. There are two routes; see *Storage
model* for why the permission is unavoidable.

**Route A — any folder on shared storage.** Put a legally obtained Umineko
Project installation in a folder you create — not `Download` itself, and not the
top level of internal storage, as Android refuses to hand those to the picker.
Launch the app, grant all-files access, and select the folder.

**Route B — the app-scoped directory.** Needs no permission, but is hidden from
most file managers:

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

#### Letting Gradle run make for you

Gradle cannot *build* the engine, but it can shell out to `make`, which is
already incremental — after the initial dependency build a single `.cpp` change
takes seconds. Enable it with `-PbuildEngine`, or permanently:

```properties
# Resources/Droid/gradle.properties
ons.buildEngine=true
```

Then Run in the IDE compiles C++ and packages in one step. It is **off by
default** because the first build takes hours, and a Gradle task that long is
hostile inside an IDE — run that one from a terminal.

On Windows the task invokes MSYS2's own `bash` with `MSYSTEM=UCRT64`, because the
dependency recipes need MSYS2's autotools, cmake, meson, ninja and nasm. It
deliberately does not use Git Bash: mixing that with MSYS2's `make` loads two
`msys-2.0.dll` copies and misbehaves. Overrides are `-Pons.msys2=<root>` and
`-Pons.makeArgs=-j8`.

It still requires `configure` to have been run once, since that is what generates
the Makefile.
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

Completed packages are stamped below each target's
`Dependencies/onscrlib/.pkgs/<name>` and skipped on later runs, so the cost is
paid once. Android stamps include the package version, ABI, API floor, and NDK
wrapper version; changing any of those inputs forces a rebuild instead of
reusing an incompatible static archive. Building only `arm64` roughly halves
the initial cost; `x86_64` is emulator-only.

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

### Windows pitfalls

Three environment problems cause failures that look like something else. All
three were hit on a clean machine.

**Run inside MSYS2 with a clean PATH.** If Git for Windows' `usr/bin` precedes
MSYS2's on PATH, `configure` picks up Git's `expr`, which mangles the
backslashes in a BRE when invoked from an MSYS2 process. `expr` then returns `0`
instead of the matched value, so `--droid-arch=arm64` is parsed as arch `0`:

```
Error: unsupported droid arch '0'.
```

Launch with `MSYS2_PATH_TYPE=strict` so the Windows PATH is not inherited, or use
a real MSYS2 shell. `configure` no longer uses `expr` for this, but other scripts
may still be affected, so a clean PATH remains the rule.

**Do not extract the NDK with Info-ZIP `unzip`.** UnZip 6.00 silently applies
LF-to-CRLF translation to the NDK's executables. `clang.exe` comes out ~300 KB
larger than the archive records and Windows refuses to load it:

```
cannot execute binary file: Exec format error      # from a shell
This app can't run on your PC                      # from Explorer
```

The `MZ` header and `file` output still look valid, so it reads as a wrong
architecture or permissions problem rather than corruption. Compare the extracted
size against `unzip -l` to confirm. `ndktoolchain.sh` now prefers `bsdtar`, which
is correct and roughly an order of magnitude faster.

**Pass an absolute path to `ndktoolchain.sh`.** The generated wrappers embed the
path they were given, so a relative one yields wrappers that only resolve from
one directory. The script now absolutises its argument.

### Gradle wrapper provenance

`gradle/wrapper/gradle-wrapper.jar` is an executable committed to the repository,
so it is a supply-chain surface. It is generated from the pinned distribution and
should be regenerated, never hand-copied:

```sh
cd Resources/Droid
./gradlew wrapper --gradle-version <ver> --gradle-distribution-sha256-sum <sha>
```

`gradle-wrapper.properties` pins `distributionSha256Sum`, which the wrapper
verifies before unpacking, and sets `validateDistributionUrl=true`. Verify the
pin matches the cached download with
`sha256sum ~/.gradle/wrapper/dists/gradle-*/*/gradle-*-bin.zip`.

Adding `gradle/actions/wrapper-validation` to CI would check the jar against
Gradle's published known-good checksums on every push.

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
| minSdk / targetSdk / compileSdk | 30 / 36 / 36 |
| ABIs | `arm64-v8a`, `x86_64` |
| NDK | r29 (`29.0.14206865`) |
| Java | 17 |
| AGP | 9.2.0, Gradle 9.4.1 |
| Renderer | SDL3 `SDL_GPU` (Vulkan) — no GLES fallback exists |

`armeabi-v7a` and `x86` are not supported.

The renderer requires baseline Vulkan support, not a particular GPU vendor.
Android device creation disables SDL's optional clip-distance, depth-clamping,
indirect-first-instance, and anisotropy requirements because the engine does
not use those features. Requiring SDL's defaults would reject otherwise capable
Android GPUs before a window was created.

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
| `getArguments()` | `{"--root", <scoped path>, "--hwdecoder", "off"}` | Selects storage and avoids unrecoverable MediaCodec loss after backgrounding |

### The Java layer is not a launcher

It is tempting to read `SDLActivity` as a thin shim that opens a native
application. It is not. Android exposes no way for native code to obtain a
window, input events or an audio device on its own, so the Java side owns all of
it and bridges back over JNI. Current size:

| File | Lines | Native methods |
| --- | --- | --- |
| `SDLActivity.java` | 2260 | 56 |
| `SDLControllerManager.java` | 1010 | 10 |
| `HIDDeviceBLESteamController.java` | 829 | 1 |
| `HIDDeviceManager.java` | 698 | 8 |
| `SDLSurface.java` | 464 | 0 |
| `HIDDeviceUSB.java` | 354 | 0 |
| `SDLInputConnection.java` | 135 | 2 |
| `SDLAudioManager.java` | 126 | 3 |
| `SDL.java` | 90 | 2 |
| `ONSActivity.java` | 326 | 1 |
| `SDLDummyEdit.java` | 65 | 0 |
| `SDLSensorManager.java` | 31 | 0 |
| `HIDDevice.java` | 21 | 0 |

That is ~6400 lines and 82 native entry points covering the rendering surface,
touch/key/mouse/gamepad input, sensors, IME and soft keyboard, audio device
lifecycle, USB and Bluetooth HID, clipboard, permissions, and translation of the
activity lifecycle into SDL events. Treat it as a port layer, not glue.

The nine launcher, storage, input, and diagnostic classes named in Tier 1 below
are project code. The SDL-prefixed activity, surface, audio, controller, HID,
and input classes are vendored SDL3 sources and should normally be replaced
wholesale on an SDL upgrade; the local message-box completion guard is the one
intentional patch and must be carried forward when that happens.

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

`nativeSetenv` **does** reach the engine. SDL3's implementation in
`src/core/android/SDL_android.c` calls POSIX `setenv()` directly, and says so:

```c
// Note that we call setenv() directly to avoid affecting SDL environments
setenv(utfname, utfvalue, 1); // This should NOT be SDL_setenv()
```

Confirmed at runtime: setting `EXTERNAL_STORAGE` to the game folder moved the
directory the engine creates for `getStorageDir()` to match, which only works if
`std::getenv` observed it.

Arguments are still preferred, for a different reason. `--root` is
*authoritative* — the engine refuses to let a later path override it, so the game
folder cannot be second-guessed by a config file or by whatever `getLaunchDir()`
derives from the environment. The environment variables remain set because
`getLaunchDir()` reads them, and they decide where the engine puts files of its
own; they are not how the game folder is located.

Related: **Android's `System.loadLibrary` uses `RTLD_NOW` without `RTLD_GLOBAL`.**
Preloading a system library from Java does not make its symbols visible to
libraries loaded afterwards, so a missing dependency cannot be papered over that
way — it must be recorded in `libmain.so`'s own `DT_NEEDED`.

## Storage model

The engine reaches the filesystem through plain POSIX calls: `FileIO::accessFile`
is a `stat(2)`, the readers are `fopen(3)`. Everything about the storage design
follows from that one fact.

### Why SAF alone cannot work

`ACTION_OPEN_DOCUMENT_TREE` returns a `content://` tree URI. That grant lives in
the DocumentsProvider layer — `ContentResolver`, `DocumentFile` — and puts
nothing in the kernel's view of the path, so the engine still cannot `stat` or
`open` anything inside it. Feeding the engine from SAF would mean copying the
data into app-scoped storage first, which for a full Umineko install means
duplicating on the order of 12 GB.

`MANAGE_EXTERNAL_STORAGE` is what actually restores POSIX access to shared
storage, and it is therefore required for any folder outside the app-scoped
directories.

The two mechanisms do different jobs and the app uses both:

| Mechanism | Provides |
| --- | --- |
| `ACTION_OPEN_DOCUMENT_TREE` | names *which* folder the user means |
| `MANAGE_EXTERNAL_STORAGE` | makes that folder readable by `stat`/`fopen` |

### The flow

`SetupActivity` is the launcher activity. It requests all-files access, runs the
system picker, converts the tree URI back to a filesystem path, verifies that
path is really readable, persists it, and only then starts `ONSActivity`.

It is a separate activity because `SDLActivity` starts the native thread from
its own lifecycle — the transition to RESUMED with a ready surface is the entry
point to the C app — and `getArguments()` is read on that thread. There is no
supported point inside that sequence at which to block for a permission dialog.
Splitting the decision out also keeps the vendored `SDLActivity` a plain
`android.app.Activity`; only `SetupActivity` depends on AppCompat.

`GameStorage.resolveTreeUri` does the URI to path conversion. External-storage
document ids are `<volume>:<relative path>`, with `primary` for built-in storage
and a UUID such as `1D03-2E0F` for a card. Any other authority — Drive, a cloud
provider — has no filesystem path and is rejected rather than guessed at.

Validation uses `dir.list() != null`, not `exists()` or `canRead()`. Both of the
latter answer from metadata and still succeed on directories that cannot be
opened, which is precisely the shared-storage case being tested for.

### What the picker refuses

Android blocks `ACTION_OPEN_DOCUMENT_TREE` from returning the root of internal
storage, the root of an SD card, and `Download` — these answer "To protect your
privacy, choose another folder". Users must create a subfolder and select that.
`SetupActivity`'s on-screen text says so.

### Fallback and recovery

With no folder configured the app falls back to the app-scoped directory:

```
/sdcard/Android/data/org.umineko_project.onscripter_ru/files/ONScripter-RU/
```

That location needs no permission at all, but on Android 13+ it is unreachable
from most on-device file managers, so filling it means `adb push` or MTP.

Only an explicitly chosen folder counts as configured. The app-scoped directory
becomes ready after the engine or `adb push` creates it; merely resolving its
path must not skip the setup screen and launch without game data.

Once a folder is set the launcher goes straight to the engine, so a wrong choice
would otherwise be unrecoverable without clearing app data. A **Change folder**
launcher shortcut (long-press the icon) re-opens `SetupActivity`. Static
shortcut intents cannot carry extras, so it is dispatched by a dedicated action
rather than a boolean. If an engine is already running, `RestartActivity` takes
over in a separate process, waits for the old process to exit, and launches a
fresh `ONSActivity`. Reusing the `singleTask` instance would leave the native
engine attached to the previous folder because `getArguments()` only runs at
engine startup.

### Where saves go

The launcher deliberately does not pass `--save`. Leaving `save_path` unset lets
`lookupSavePath()` retain the engine's per-game identifier, which prevents two
games using the same root from sharing save files.

For a user-selected root, `EXTERNAL_STORAGE` is the game folder itself.
`getLaunchDir()` appends `ONScripter-RU`, `getStorageDir()` appends `SaveData`,
and `lookupSavePath()` appends the game identifier:

```
<selected game folder>/ONScripter-RU/SaveData/<game id>/
```

The app-scoped fallback is the compatibility exception. Its root already ends
in `ONScripter-RU`, so `GameStorage.getNativeStorageBase()` passes the parent to
the native environment. This preserves the location used by existing releases:

```
/sdcard/Android/data/org.umineko_project.onscripter_ru/files/
  ONScripter-RU/SaveData/<game id>/
```

Do not point the fallback environment at the fallback root itself: native
`getLaunchDir()` would append a second `ONScripter-RU`, making all existing saves
appear to vanish after an upgrade.

### Diagnosing access from the shell

`run-as` cannot answer whether the app can read shared storage. It runs in a
restricted mount namespace that never receives the pass-through mount, so it
reports shared storage as unreadable even when the app itself reads it fine.
Confirmed on Android 15: `run-as ... ls /sdcard/Download` printed a denial while
the app process logged `listed=20 entries` for the same volume.

Use the in-app probe instead — `GameStorage.logAccessState` runs inside the app
process and logs the permission flag next to what the app can actually list.

## Android-specific code

Android work is not confined to `Resources/Droid`. It spans that directory, the
`Support/Droid` sources, `#if defined(DROID)` regions inside otherwise shared
files, and the Droid branch of `configure` — link flags in particular live there,
not in the Gradle project.

Work on this target should stay inside tiers 1 and 2.

### Tier 1 — wholly Android-only

```
Resources/Droid/**        manifest, build.gradle, gradle wrapper, res/, 21 Java sources
                          (9 project-owned launcher/input/diagnostic classes;
                           the other 12 are vendored SDL3)
Support/Droid/**          DroidProfile.cpp / .hpp
Scripts/ndktoolchain.sh   NDK discovery and wrapper toolchain generation
Scripts/apkbuild.tool     Gradle/AGP packaging
Scripts/quickdroid.tool   multi-ABI build driver
```

### Tier 2 — shared files with Android-only regions

Edit shared files only inside `#if defined(DROID)` guards unless a platform-
neutral refactor is independently verified:

```
Engine/Media/HardwareDecoder.cpp     MediaCodec hwaccel, JNI vm registration
Engine/Media/VideoDecoder.cpp, Controller.hpp
Engine/Graphics/GPU.cpp, GPU.hpp, SDL3GPUCompat.cpp, SDL3GPUCompat.hpp
Engine/Core/ONScripter.cpp, Command.cpp, CommandExt.cpp
Engine/Core/Event.cpp, Loader.cpp, Animation.cpp
Engine/Components/Window.cpp, Window.hpp
Support/FileIO.cpp                   storage paths, __android_log logging
External/Compatibility.hpp
```

Build files with Android-only regions: the `*clang*:"Droid")` branch of
`configure`, and `configopts_droid` / `cflags_droid` blocks in
`Dependencies/pkgs/*.pkgbuild`.

### Tier 3 — do not touch for Android work

Everything else, including unguarded shared renderer code, `Tests/`,
`Resources/Windows/` and `Support/Apple/`.

## Debugging

### Log tags

| Tag | Source |
| --- | --- |
| `ONSJava` | every Java class this project owns, via `Diag` |
| `ONScripter-RU` | engine, via `FileIO::log` and `__android_log_vprint` |
| `SDL` | SDL3's Java and native layers |
| `nativeloader`, `System.err` | dynamic linker — **where real `dlopen` failures appear** |
| `libc`, `DEBUG` | native crashes (`Fatal signal 11`) and tombstones |

One tag per side of the JNI boundary, so a single filter shows the whole startup
and it is never ambiguous which half produced a line:

```sh
adb logcat -s ONSJava:V ONScripter-RU:V SDL:V
```

The handoff is explicit. `ONSActivity` logs the exact `argv` immediately before
`nativeRunMain`, and the engine's first line follows it:

```
ONSJava      : ONSActivity: handing off to engine, argv [--root, /storage/.../ONScripter-RU]
ONScripter-RU: Launched with pid 10854, previous pid 0
ONScripter-RU: set:archive_path: "/storage/.../ONScripter-RU/"
ONScripter-RU: Invalid launch directory!
```

If the `ONScripter-RU` tag never appears after the handoff line, `main()` was
never reached and the fault is in loading, not in the engine.

### Engine logging on Android

Engine output reaches logcat by default: `FileIO::log` routes through
`__android_log_vprint` under the provider name, mapping `LogLevel::Info/Warn/Error`
onto `I/W/E`. `performTerminate` logs through the same path before showing its
message box, so fatals are visible — the "Invalid launch directory!" line above
is that path.

Two things to know:

**`--use-logfile` turns logcat output off.** The Android branch in `FileIO::log`
is guarded by `logMode != LogMode::File`, so file mode falls through to
`stdout`/`stderr` instead. Those are reopened onto `out.txt` and `err.txt` in
`getStorageDir()` — see *Where saves go* for what that resolves to. Development builds
(`#ifndef PUBLIC_RELEASE`) set `LogMode::Console`, which still reaches logcat.

**Java-side crashes are logged separately.** `Diag.installCrashHandler`, wired up
from `ONSApplication`, logs uncaught exceptions on `ONSJava`, writes a report,
and opens `CrashActivity` in a separate process before terminating the failed
process. It cannot see native crashes; those are recovered on the next launch
through `ApplicationExitInfo`.

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
with `d8 --min-api 30`, replace `classes.dex` inside a copy of the APK, then
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

## Backlog

Outstanding work only. Anything finished is deleted from here rather than marked
done — the git log is the record of what was done.

### Defects

**Hardware video decode does not survive backgrounding.** Android hands out
`MediaCodec` instances from a small global pool and reclaims them from apps in
the background. The engine cannot survive that: it never releases the codec on
pause, and it cannot rebuild a decoder mid-playback — looping only seeks the
demuxer and deliberately keeps the codec alive (`Engine/Media/Demux.cpp`, "we
don't need to flush codec buffers in that case"). Every call into the dead codec
then returns `AVERROR_EXTERNAL`, which `Decoder::receiveAvailableFrames` treats
as recoverable and reports as success, so `sendPacket`'s `EAGAIN` branch spins on
it forever: black screen, ~140% CPU across two threads, ~15k log lines a second,
no recovery short of a restart.

Worked around by passing `--hwdecoder off`, so no `MediaCodec` is ever created.
The real fix is to release the codec on background and rebuild it on resume,
seeking back to the paused position — which needs three things the media layer
does not have: decoder re-creation, seeking to an arbitrary timestamp (only
seek-to-zero-for-loop exists), and re-syncing video against audio and subtitles.

Independently, a decoder that fails should not be reported as success. Treating a
fatal error as recoverable is what turns a dead codec into an unbounded spin, and
would do the same for any other fatal decode error.

**One stale frame when a window resizes.** The resize is handled on the event,
but a frame can already be in flight with the previous canvas, so entering a
floating window shows a single frame at the old geometry before correcting.
Observed as `Swapchain 1920x1080, target 1920x1371` immediately before the
matching `Surface resize` line. Cosmetic, and it would read as an intermittent
glitch if found later without this note.

**`VK_ERROR_SURFACE_LOST_KHR` on cold start with the screen off.** Launching
against a sleeping display can fail renderer init outright. Racy — observed once,
then not reproduced across two deliberate attempts at 27 s and 70 s asleep. Same
family as the resize bug: started before the display was ready.

**Teardown overruns the join and aborts.** SDL parks the engine thread in
`nativePause()` *before* `onDestroy` sends the quit, so `mSDLThread.join(1000)`
in `SDLActivity` expires and the engine's shutdown — async queues, GPU release —
runs on after the window is destroyed, aborting in `hwuiTask1` with `FORTIFY:
pthread_mutex_lock called on a destroyed mutex`. Intermittent, and likelier the
more there is to tear down. Not reachable via Back any more, still reachable by
swiping the app from recents.

**`CrashReport.isAbnormal()` misses `REASON_SIGNALED`.** A Java fatal that the
platform then SIGKILLs is recorded as `reason=2 (SIGNALED) status=9` and never
reported. Note a Back-triggered exit records as `reason=1 (EXIT_SELF)`, so
crashes during a voluntary exit are invisible by design.

**Graphics memory is ~370 MB and only a quarter of it is accounted for.** With
the game idle on the title screen `dumpsys meminfo` reports 368 MB under
`GL mtrack`, while the engine's own census of every live `GPU_Image` totals
88–125 MB across 9–13 images, pooled render targets included, and 0 KB of CPU
side pixel copies. The engine's textures are therefore not the problem — the
largest single one is a 32 MB 2048×4096 render target and the rest are canvas
and script sized targets, all plausible.

That leaves roughly 250–280 MB allocated below the engine, in SDL_GPU or the
Mali driver, and unattributed. Known candidates worth measuring before anything
is changed: the swapchain (2800×2000×4 is 22 MB an image, and there are at least
three), the two `SDL3GPUReusableTransferBuffer` staging buffers, which grow to
the largest upload ever made and are never shrunk — a 32 MB texture leaves a
32 MB buffer behind — and whether `imagePixelBytes` counts mip levels.

This matters because it is device memory: it cannot be swapped or compressed,
which is why ColorOS logs `osense.compress ... cur ratio = 0` against this
process every 20 s and reclaims nothing, and why the process is the first thing
the low memory killer reaches for.

**Resume can stay black for several seconds.** Returning from another app, one
observation showed ~6 s between `did enter foreground` and `Swapchain rebuilt
after resume`. The rebuild happens on the first flip after resume, and an idle
screen has nothing to flip. Single observation, and worth re-measuring now that a
failed swapchain reclaim is retried rather than abandoned.

### Features

**Touch targets are below Android's minimum.** Everything renders into a fixed
1920×1080 script space scaled uniformly to the window, and button geometry is
hardcoded in the game script — `spbtnCommand` just takes the sprite's rect. On a
420 dpi phone script pixels are device pixels, so `saveload_area_n1_button.png`
gives an 80×78 px target, 5.0 mm, against the 48 dp / 126 px / 7.6 mm minimum. A
400 dpi tablet already satisfies it — measured 117 px against a 120 px minimum on
the OnePlus Pad — so any fix must be driven by **DPI, not resolution**, and the
engine has no DPI awareness at all, so Java has to pass
`DisplayMetrics.densityDpi` in.

Two workable levers. Inflating hit targets in `mouseOverCheck` is the cheap one:
a single chokepoint, run exact-rect first and inflated only on a miss so precise
taps are unchanged and no button can steal another's click; mind that `transbtn`
screens alpha-test against `select_rect`. Scaling `preset_define` font sizes is
the bigger win — UI labels are text sprites and `align_buttons_r` re-flows them
via `getspsize` — but presets are shared with story text and `wrap_limit` /
`line_height` must scale in step, so it needs a per-preset allow-list.

Scaling the image-based buttons is not feasible: hardcoded coordinates and baked
PNG sizes with no spacing to absorb growth. That is the ceiling.

**Autosave.** The engine snapshots state to RAM at every text page but writes to
disk only on an explicit slot save, so a crash or an OS kill costs everything
since the player's last manual save. More pressing on Android, where being killed
while backgrounded is routine rather than exceptional.

### Device compatibility

The target is every Android 11 (minSdk 30) device, not just whatever is on the
desk. Real coverage today is two devices: a Snapdragon/Adreno phone and a
MediaTek/Mali tablet.

That second device is the argument for the rest of this section. Until it was
plugged in, the port failed to start on **every Mali GPU** — all Exynos and most
MediaTek parts — with a fatal dialog, for the reason described under *Supported
target*. A whole GPU vendor was excluded, and it was found by chance rather than
by testing. Assume other such gaps exist.

**Verify API 30 on a device that actually runs it.** The floor is now 30, but
both test devices are Android 15/16, so what has been shown is that an
API-30-targeted binary runs on modern Android -- not that it runs on Android 11.
The `onBackPressed` path added for 30-32 cannot execute on either device either,
since both take the 33+ dispatcher route. Needs an emulator image in that range.

`sw600dp` selects the phone/tablet split, and it measures the **window**, not the
device. In split screen the tablet resolves `sw534dp` -- the phone bucket -- and
a foldable changes bucket when it folds. Today that is harmless because
`lock_landscape` is read once in `onCreate` while fullscreen, but the qualifier
is a proxy for the question actually being asked, which is whether the portrait
band would still be large enough to touch. Deriving it from the rendered band's
physical size would say what is meant.

Worth building a device matrix and working through it deliberately:

- **GPU vendor** — Adreno and Mali are covered. PowerVR and Samsung Xclipse are
  not, and each has its own view of which optional Vulkan features exist.
- **Android version** — 11–13 still need coverage, while 14, 15 and 16 already
  behave differently in ways that matter: `enableOnBackInvokedCallback` changes
  how Back is delivered on 33+, and 16 ignores manifest-declared fixed
  orientation on large screens.
- **Form factor** — phone, tablet, foldable, TV. Touch-target sizing and
  orientation handling differ; a TV has no touch at all.
- **ABI** — only arm64-v8a has ever been run. x86_64 builds but is untested.
- **Density** — drives the touch-target work above; 400–420 dpi is all that has
  been measured.

Two practical obstacles found while doing this. Some OEM builds suppress app
logcat output entirely — ColorOS sets `ro.oplus.log.enable=false`, and without
root neither `setprop log.tag.*` nor `run-as logcat` recovers it — so
`--use-logfile` (writing `out.txt` / `err.txt` into the storage directory) is the
only way to see engine output there. And game data must be present per device,
with `game.hash` matching the engine's `ONS_VERSION`, which is read from the
shipped binary rather than the source tree.

### Infrastructure

- CI (`.github/workflows/build.yml`) covers Windows and Linux only. There is no
  Android build or smoke test, so Android regressions are caught by hand — which
  is how a whole-GPU-vendor failure reached a merged PR.
- Unit tests exist (`Tests/CMakeLists.txt`,
  `Resources/Droid/test/GameStorageTest.java`), but no deterministic game-data
  regression corpus does, which limits confidence in renderer and media changes.
  `Tests/Fixtures/SmokeGame/0.txt` is a minimal script, not a runnable game — the
  engine still reports `Invalid launch directory!` with only that present.
- `android:screenOrientation="sensorLandscape"` is ignored on targetSdk 36;
  Android 16 drops manifest-declared fixed orientation on large screens.
- `Resources/Droid/gradle/gradle-daemon-jvm.properties` is untracked and
  undecided. Gradle generates it, and it pins JDK 25 with foojay download URLs,
  which would impose that toolchain on every contributor. Commit or gitignore.

### Unverified

Behaviours never exercised, as opposed to the hardware axes above: long
backgrounding where Android kills the process outright; incoming phone calls;
split screen; and whether the three-finger swipes reach the intended engine state
in every mode — they fire and are logged, which is all that has been confirmed.
