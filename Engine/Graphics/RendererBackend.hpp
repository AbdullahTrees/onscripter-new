/**
 *  RendererBackend.hpp
 *  ONScripter-RU
 *
 *  Legacy renderer backend boundary.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "Support/SDLCompat.hpp"

#if defined(ONS_USE_SDL3)
// SDL3_GPU is exposed through a SDL2_gpu-shaped transition surface while the
// higher-level renderer code is being ported off SDL2_gpu semantics.
#define ONS_RENDERER_BACKEND_SDL3_GPU 1
#include "Engine/Graphics/SDL3GPUCompat.hpp"
#else
// Keep direct SDL_gpu includes behind this header outside backend-specific code.
#define ONS_RENDERER_BACKEND_SDL2_GPU 1
#include <SDL2/SDL_gpu.h>
#endif

using RenderBatchFlag = GPU_BatchFlagEnum;
using RenderBlendMode = GPU_BlendMode;
using RenderDriverId = GPU_RendererID;
using RenderFileFormat = GPU_FileFormatEnum;
using RenderFormat = GPU_FormatEnum;
using RenderImage = GPU_Image;
using RenderInitFlags = GPU_InitFlagEnum;
using RenderRect = GPU_Rect;
using RenderShaderBlock = GPU_ShaderBlock;
using RenderShaderType = GPU_ShaderEnum;
using RenderTarget = GPU_Target;
using RenderWindowFlags = GPU_WindowFlagEnum;
