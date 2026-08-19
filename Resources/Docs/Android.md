# Android

Reference for the Android target: how the native and Java halves fit together,
the contracts between them, and the environment the build assumes.

This documents current architecture, not change history — for what changed and
when, read the git log.

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

There are no prebuilt library bundles to download — `onscrlib` is a meta-package
listing dependencies, and releases ship only the APK and a Windows zip.

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

## Working in Android Studio

Open **`Resources/Droid`** as the project, not the repository root.

Gradle does not build the engine and is not intended to — the native side is a
17-package source build driven by `configure`/`make`. The `syncEngineLibs` task
in `build.gradle` copies `DerivedData/Droid-<arch>/onscripter-new` into
`lib/<abi>/libmain.so` and runs before every build, so the IDE handles only
packaging, install, run and debug.

One-time: install **NDK 29.0.14206865** and the API 36 platform via SDK Manager
(AGP needs the NDK to strip native libraries even though it does not compile
them), then export `ANDROID_SDK_ROOT`.

Then build the engine once from a terminal and press **Run**. Java changes need
no native rebuild; after a C++ change re-run `make` and press Run again — the
copy task picks up the newer binary. For native breakpoints set the run
configuration's debugger to **Dual**; symbols come from the unstripped binary in
`DerivedData`.

`Scripts/apkbuild.tool` remains the path for reproducible command-line and
release packaging, staging a copy of this same project under
`DerivedData/Droid-package`.

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
