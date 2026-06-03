onscripter-new
===============

`onscripter-new` is a modernization branch of ONScripter-RU for the Umineko
Project engine. It keeps the project-specific visual novel runtime and script
compatibility surface, while moving the build, dependency, renderer, and audio
stack onto current toolchains.

This repository is not a general-purpose visual novel SDK. The engine is still
shaped around Umineko Project's release data, compatibility requirements, and
runtime behavior. It has many project-specific paths and does not provide a
stable public authoring API.

Current focus
-------------

- SDL3 is the default and only renderer stack in the active configure path.
- Windows public releases use SDL3_GPU, SDL3_image, and SDL3_mixer through the
  static `onscrlib` dependency build.
- The old SDL2_gpu/libepoxy fallback, legacy GL/GLES backends, and ANGLE/GLES
  Windows renderer DLL packaging have been removed from the active build.
- The local Windows target is x86_64 MSYS2/UCRT64 with a Windows 10 runtime
  floor.
- Renderer work is tracked through SDL3_GPU benchmarks and optional runtime
  telemetry for uploads, readbacks, command buffers, native shader paths, and
  CPU fallbacks.

Building
--------

For a normal host build:

```sh
./configure
make -j8
```

For the current Windows public-release build used with packaged Umineko Project
data:

```sh
./configure --release-build --strip-binary --std=gnu++14
make -j8
```

Development builds expect plaintext script layouts. Packaged game distributions
that ship compressed `.file` scripts should use a public release build.

Documentation
-------------

- [Compilation guide](Resources/Docs/Compilation.md)
- [Dependency audit and modernization plan](Resources/Docs/DependencyAudit.md)
- [SDL3 performance audit](Resources/Docs/SDL3PerformanceAudit.md)

Credits
-------

- Ogapee
- "Uncle" Mion Sonozaki
- Umineko Project
- All third-party library authors
