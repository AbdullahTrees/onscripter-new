Windows release builds link SDL3, SDL3_image, and SDL3_mixer statically.
No SDL, ANGLE, EGL, GLES, or d3dcompiler renderer DLLs are part of the
normal package.

The remaining files in this directory are optional crash-reporting helpers.
Place the dlls directory next to the executable only when that debugging
support is desired.
