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
./configure --release-build --strip-binary --std=gnu++14
make -j8
```

On current MSYS2/UCRT64 GCC 16 toolchains, `--std=gnu++14` is recommended for Windows release builds. It avoids strict `-std=c++14` parsing failures in current libstdc++ headers around `__float128` literal suffix declarations.

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
Project main menu with `--release-build --strip-binary --std=gnu++14`; the log
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
2. Legacy custom-clang, macOS 10.6, i386, armv7, and armv7s targets are no longer supported. A custom compiler is no longer required for the supported floor.
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
- openssl command line tool
- zip command line tool (pacman -S zip in msys2)
- wget or curl command line tool
- libtool for libunwind compilation
- JDK 17+ for APK packaging
- Android SDK platform 36, Build Tools 36.1.0, Platform Tools, and NDK r29

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

**Multiple architecture compilation guide**:

To compile for multiple architectures (i.e. create a FAT apk file) for deployment you could either use `./Scripts/quickdroid.tool` tool or run the following commands manually:
```
./configure --droid-build --droid-arch=arm64
make
./configure --droid-build --droid-arch=x86_64
make
make apkall
```

`./Scripts/quickdroid.tool` accepts the following arguments:
- `--normal` — normal developer build (default)
- `--release` — stripped release build
- `--debug` — debug build

**Debugging the binaries**:

It is recommended to debug using IDA Pro.

1. Setting Java debugger in order to properly start the application. It is worth checking the [official documentation](https://www.hex-rays.com/products/ida/support/tutorials/debugging_dalvik.pdf) first.
   
    1. Open classes.dex in (32-bit) IDA Pro by dragging onscripter-ru.apk into its main window
    2. Put a breakpoint on `_def_Activity__init_@V`
    3. Go to `Debugger` → `Debugger options` → Set specific options and fill adb path
    4. Launch the debugger and specify source path mapping (`.` → `path/to/onscripter/sources`)

2. Setting hardware debugger in order to debug the binary.

    1. Open `libmain.so` in IDA Pro by dragging `onscripter-ru.apk` into its main window
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
            /data/app-lib/org.umineko_project.onscripter_ru-1/libmain.so
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

- Java, Android SDK Build Tools, Android platform 36, and NDK r29 are required for supported APK packaging.
- Only arm64-v8a and x86_64 binaries are compiled.
- Building on Linux and Windows systems is mostly untested
- Building standalone onscrlib package may fail on Windows due to `%PATH%`/`$PATH` design
- Source level debugging may not always be available
- The logs are generated with ONScripter-RU and SDL tags:
```
adb logcat | grep -E '(ONScripter-RU|SDL)'
```

#### Building Android Java sources

Even though all the Java-dependent files are provided in compiled form you may rebuild them.

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

5. Recompile the resources by running the following command:
    ```
    ./Scripts/apkbuild.tool DerivedData
    ```

    The following arguments are supported:

    - `--jsign` — signs apk file with jarsigner

    The following environment variables are supported:

    - `JAVA_PATH` — path to `bin/javac`
    - `DROID_TOOLS` — path to Android build-tools
    - `DROID_PLATFORM` — path to `android.jar`

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
