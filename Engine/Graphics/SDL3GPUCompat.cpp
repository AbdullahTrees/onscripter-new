/**
 *  SDL3GPUCompat.cpp
 *  ONScripter-RU
 *
 *  SDL2_gpu-shaped compatibility surface for the SDL3_GPU renderer path.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Graphics/SDL3GPUCompat.hpp"

#if defined(ONS_USE_SDL3)

#include "Engine/Graphics/GPU.hpp"
#include "Engine/Graphics/SDL3GPUShaders/SDL3GPUShaders.hpp"
#include "Support/FileDefs.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <png.h>

#if defined(ONS_USE_SDL3_SHADERCROSS)
#include <SDL3_shadercross/SDL_shadercross.h>
#if defined(ONS_USE_SDL3_SHADERC)
#include <shaderc/shaderc.h>
#endif
#endif

namespace {
GPU_Renderer rendererState{};
GPU_InitFlagEnum pendingPreinitFlags{GPU_DEFAULT_INIT_FLAGS};
char shaderMessage[256]{"SDL3_GPU compatibility layer is active"};
Uint32 nextShaderObject{1};

struct SDL3GPUVertex {
	float x;
	float y;
	float r;
	float g;
	float b;
	float a;
	float s;
	float t;
};

struct SDL3GPUShaderBytecode {
	const Uint8 *code{nullptr};
	size_t size{0};
	SDL_GPUShaderFormat format{SDL_GPU_SHADERFORMAT_INVALID};
	const char *entrypoint{"main"};
};

struct SDL3GPUPipelineEntry {
	SDL_GPUTextureFormat targetFormat{SDL_GPU_TEXTUREFORMAT_INVALID};
	GPU_bool useBlending{false};
	GPU_BlendMode blendMode{};
	SDL_GPUShader *vertexShader{nullptr};
	SDL_GPUShader *fragmentShader{nullptr};
	SDL_GPUGraphicsPipeline *pipeline{nullptr};
};

enum class SDL3GPUShaderKind {
	Unknown,
	DefaultVertex,
	AlphaOutsideTextures,
	BlendByMask,
	BlurH,
	BlurV,
	Breakup,
	ColorModification,
	ColourConversion,
	CropByMask,
	EffectTrvswave,
	EffectWarp,
	EffectWhirl,
	GlassSmash,
	GlyphGradient,
	MergeAlpha,
	MultiplyAlpha,
	Pixelate,
	RenderSubtitles,
	TextFade
};

enum class SDL3GPUUniformType {
	Int,
	Float,
	FloatVec
};

struct SDL3GPUNativeResourceInfo {
	Uint32 numSamplers{0};
	Uint32 numStorageTextures{0};
	Uint32 numStorageBuffers{0};
	Uint32 numUniformBuffers{0};
};

struct SDL3GPUNativeUniform {
	std::string name;
	SDL3GPUUniformType type{SDL3GPUUniformType::Float};
	int components{1};
	int arraySize{1};
	Uint32 registerIndex{0};
};

struct SDL3GPUNativeUniformRegister {
	Uint32 words[4]{0, 0, 0, 0};
};

struct SDL3GPUShaderObject {
	GPU_ShaderEnum type{GPU_FRAGMENT_SHADER};
	SDL3GPUShaderKind kind{SDL3GPUShaderKind::Unknown};
	std::string source;
	SDL_GPUShader *nativeShader{nullptr};
	SDL3GPUNativeResourceInfo nativeResources{};
	std::vector<SDL3GPUNativeUniform> nativeUniforms;
	bool translatedLegacyGLSL{false};
};

struct SDL3GPUUniformValue {
	SDL3GPUUniformType type{SDL3GPUUniformType::Float};
	int intValue{0};
	float values[4]{0.0f, 0.0f, 0.0f, 0.0f};
	int components{1};
};

struct SDL3GPUProgramObject {
	SDL3GPUShaderKind kind{SDL3GPUShaderKind::Unknown};
	std::vector<Uint32> shaders;
	std::unordered_map<std::string, int> uniformLocations;
	std::unordered_map<int, std::string> locationNames;
	std::unordered_map<std::string, SDL3GPUUniformValue> uniforms;
	std::array<GPU_Image *, 8> images{};
	SDL_GPUShader *nativeVertexShader{nullptr};
	SDL_GPUShader *nativeFragmentShader{nullptr};
	SDL3GPUNativeResourceInfo nativeFragmentResources{};
	std::vector<SDL3GPUNativeUniform> nativeUniforms;
	std::unordered_map<std::string, size_t> nativeUniformLookup;
	std::vector<SDL3GPUNativeUniformRegister> nativeUniformRegisters;
};

struct SDL3GPUColorF {
	float r{0.0f};
	float g{0.0f};
	float b{0.0f};
	float a{0.0f};
};

SDL_GPUShader *texturedVertexShader{nullptr};
SDL_GPUShader *texturedFragmentShader{nullptr};
SDL_GPUSampler *nearestSampler{nullptr};
SDL_GPUSampler *linearSampler{nullptr};
std::vector<SDL3GPUPipelineEntry> pipelineCache;
std::unordered_map<Uint32, SDL3GPUShaderObject> shaderObjects;
std::unordered_map<Uint32, SDL3GPUProgramObject> programObjects;
std::unordered_map<int, Uint32> uniformLocationOwners;
int nextUniformLocation{1};
#if defined(ONS_USE_SDL3_SHADERCROSS)
bool shaderCrossInitialized{false};
#endif

void setShaderMessage(const char *message);

int textureBytesPerPixel(GPU_FormatEnum format) {
	switch (format) {
		case GPU_FORMAT_LUMINANCE:
			return 1;
		case GPU_FORMAT_LUMINANCE_ALPHA:
			return 2;
		case GPU_FORMAT_RGB:
		case GPU_FORMAT_RGBA:
		default:
			return 4;
	}
}

SDL_GPUTextureFormat textureFormat(GPU_FormatEnum format) {
	switch (format) {
		case GPU_FORMAT_LUMINANCE:
			return SDL_GPU_TEXTUREFORMAT_R8_UNORM;
		case GPU_FORMAT_LUMINANCE_ALPHA:
			return SDL_GPU_TEXTUREFORMAT_R8G8_UNORM;
		case GPU_FORMAT_RGB:
		case GPU_FORMAT_RGBA:
		default:
			return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	}
}

bool isNativeTextureFormat(GPU_FormatEnum format) {
	return format == GPU_FORMAT_RGB || format == GPU_FORMAT_RGBA;
}

bool sameBlendMode(const GPU_BlendMode &a, const GPU_BlendMode &b) {
	return a.source_color == b.source_color &&
	       a.dest_color == b.dest_color &&
	       a.source_alpha == b.source_alpha &&
	       a.dest_alpha == b.dest_alpha &&
	       a.color_equation == b.color_equation &&
	       a.alpha_equation == b.alpha_equation;
}

SDL_GPUBlendFactor toSDLBlendFactor(GPU_BlendFuncEnum factor) {
	switch (factor) {
		case GPU_FUNC_ZERO:
			return SDL_GPU_BLENDFACTOR_ZERO;
		case GPU_FUNC_ONE:
			return SDL_GPU_BLENDFACTOR_ONE;
		case GPU_FUNC_SRC_COLOR:
			return SDL_GPU_BLENDFACTOR_SRC_COLOR;
		case GPU_FUNC_DST_COLOR:
			return SDL_GPU_BLENDFACTOR_DST_COLOR;
		case GPU_FUNC_ONE_MINUS_SRC:
			return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
		case GPU_FUNC_ONE_MINUS_DST:
			return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR;
		case GPU_FUNC_SRC_ALPHA:
			return SDL_GPU_BLENDFACTOR_SRC_ALPHA;
		case GPU_FUNC_DST_ALPHA:
			return SDL_GPU_BLENDFACTOR_DST_ALPHA;
		case GPU_FUNC_ONE_MINUS_SRC_ALPHA:
			return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		case GPU_FUNC_ONE_MINUS_DST_ALPHA:
			return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA;
		default:
			return SDL_GPU_BLENDFACTOR_ONE;
	}
}

SDL_GPUBlendOp toSDLBlendOp(GPU_BlendEqEnum equation) {
	switch (equation) {
		case GPU_EQ_SUBTRACT:
			return SDL_GPU_BLENDOP_SUBTRACT;
		case GPU_EQ_REVERSE_SUBTRACT:
			return SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
		case GPU_EQ_ADD:
		default:
			return SDL_GPU_BLENDOP_ADD;
	}
}

SDL3GPUShaderBytecode selectShaderBytecode(SDL_GPUShaderFormat supported, GPU_ShaderEnum stage) {
	if (supported & SDL_GPU_SHADERFORMAT_DXIL) {
		if (stage == GPU_VERTEX_SHADER)
			return SDL3GPUShaderBytecode{tri_texture_vert_dxil, sizeof(tri_texture_vert_dxil), SDL_GPU_SHADERFORMAT_DXIL, "main"};
		return SDL3GPUShaderBytecode{texture_rgba_frag_dxil, sizeof(texture_rgba_frag_dxil), SDL_GPU_SHADERFORMAT_DXIL, "main"};
	}
	if (supported & SDL_GPU_SHADERFORMAT_SPIRV) {
		if (stage == GPU_VERTEX_SHADER)
			return SDL3GPUShaderBytecode{tri_texture_vert_spv, sizeof(tri_texture_vert_spv), SDL_GPU_SHADERFORMAT_SPIRV, "main"};
		return SDL3GPUShaderBytecode{texture_rgba_frag_spv, sizeof(texture_rgba_frag_spv), SDL_GPU_SHADERFORMAT_SPIRV, "main"};
	}
	if (supported & SDL_GPU_SHADERFORMAT_MSL) {
		if (stage == GPU_VERTEX_SHADER)
			return SDL3GPUShaderBytecode{tri_texture_vert_msl, sizeof(tri_texture_vert_msl), SDL_GPU_SHADERFORMAT_MSL, "main0"};
		return SDL3GPUShaderBytecode{texture_rgba_frag_msl, sizeof(texture_rgba_frag_msl), SDL_GPU_SHADERFORMAT_MSL, "main0"};
	}
	return SDL3GPUShaderBytecode{};
}

SDL_GPUShader *createNativeShader(GPU_ShaderEnum shaderType) {
	if (!rendererState.device)
		return nullptr;

	const SDL_GPUShaderFormat supported = SDL_GetGPUShaderFormats(rendererState.device);
	const SDL3GPUShaderBytecode bytecode = selectShaderBytecode(supported, shaderType);
	if (!bytecode.code || bytecode.format == SDL_GPU_SHADERFORMAT_INVALID) {
		setShaderMessage("SDL3_GPU backend does not expose a supported fixed shader format");
		return nullptr;
	}

	SDL_GPUShaderCreateInfo shaderInfo{};
	shaderInfo.code_size = bytecode.size;
	shaderInfo.code      = bytecode.code;
	shaderInfo.entrypoint = bytecode.entrypoint;
	shaderInfo.format    = bytecode.format;
	shaderInfo.stage     = shaderType == GPU_VERTEX_SHADER ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
	shaderInfo.num_uniform_buffers = 1;
	shaderInfo.num_samplers = shaderType == GPU_VERTEX_SHADER ? 0 : 1;

	SDL_GPUShader *shader = SDL_CreateGPUShader(rendererState.device, &shaderInfo);
	if (!shader)
		setShaderMessage(SDL_GetError());
	return shader;
}

bool ensureNativeShaders() {
	if (texturedVertexShader && texturedFragmentShader)
		return true;

	texturedVertexShader = createNativeShader(GPU_VERTEX_SHADER);
	if (!texturedVertexShader)
		return false;

	texturedFragmentShader = createNativeShader(GPU_FRAGMENT_SHADER);
	if (!texturedFragmentShader)
		return false;

	return true;
}

SDL_GPUSampler *getSampler(GPU_FilterEnum filter) {
	SDL_GPUSampler **slot = filter == GPU_FILTER_NEAREST ? &nearestSampler : &linearSampler;
	if (*slot)
		return *slot;

	SDL_GPUSamplerCreateInfo samplerInfo{};
	const bool nearest = filter == GPU_FILTER_NEAREST;
	samplerInfo.min_filter = nearest ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
	samplerInfo.mag_filter = nearest ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
	samplerInfo.mipmap_mode = nearest ? SDL_GPU_SAMPLERMIPMAPMODE_NEAREST : SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
	samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	*slot = SDL_CreateGPUSampler(rendererState.device, &samplerInfo);
	if (!*slot)
		setShaderMessage(SDL_GetError());
	return *slot;
}

SDL_GPUGraphicsPipeline *getPipeline(SDL_GPUTextureFormat targetFormat, GPU_bool useBlending, const GPU_BlendMode &blendMode,
                                     SDL_GPUShader *vertexShader = nullptr, SDL_GPUShader *fragmentShader = nullptr) {
	if (!vertexShader || !fragmentShader) {
		if (!ensureNativeShaders())
			return nullptr;
		vertexShader   = texturedVertexShader;
		fragmentShader = texturedFragmentShader;
	}

	for (const auto &entry : pipelineCache) {
		if (entry.targetFormat == targetFormat &&
		    entry.useBlending == useBlending &&
		    entry.vertexShader == vertexShader &&
		    entry.fragmentShader == fragmentShader &&
		    (!useBlending || sameBlendMode(entry.blendMode, blendMode))) {
			return entry.pipeline;
		}
	}

	SDL_GPUColorTargetDescription colorTarget{};
	colorTarget.format = targetFormat;
	colorTarget.blend_state.enable_blend = useBlending;
	colorTarget.blend_state.enable_color_write_mask = true;
	colorTarget.blend_state.color_write_mask = SDL_GPU_COLORCOMPONENT_R |
	                                           SDL_GPU_COLORCOMPONENT_G |
	                                           SDL_GPU_COLORCOMPONENT_B |
	                                           SDL_GPU_COLORCOMPONENT_A;
	if (useBlending) {
		colorTarget.blend_state.src_color_blendfactor = toSDLBlendFactor(blendMode.source_color);
		colorTarget.blend_state.dst_color_blendfactor = toSDLBlendFactor(blendMode.dest_color);
		colorTarget.blend_state.src_alpha_blendfactor = toSDLBlendFactor(blendMode.source_alpha);
		colorTarget.blend_state.dst_alpha_blendfactor = toSDLBlendFactor(blendMode.dest_alpha);
		colorTarget.blend_state.color_blend_op = toSDLBlendOp(blendMode.color_equation);
		colorTarget.blend_state.alpha_blend_op = toSDLBlendOp(blendMode.alpha_equation);
	}

	SDL_GPUVertexBufferDescription vertexBuffer{};
	vertexBuffer.slot = 0;
	vertexBuffer.pitch = sizeof(SDL3GPUVertex);
	vertexBuffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

	std::array<SDL_GPUVertexAttribute, 3> attributes{};
	attributes[0].location = 0;
	attributes[0].buffer_slot = 0;
	attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
	attributes[0].offset = offsetof(SDL3GPUVertex, x);
	attributes[1].location = 1;
	attributes[1].buffer_slot = 0;
	attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[1].offset = offsetof(SDL3GPUVertex, r);
	attributes[2].location = 2;
	attributes[2].buffer_slot = 0;
	attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
	attributes[2].offset = offsetof(SDL3GPUVertex, s);

	SDL_GPUGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.vertex_shader = vertexShader;
	pipelineInfo.fragment_shader = fragmentShader;
	pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pipelineInfo.vertex_input_state.vertex_buffer_descriptions = &vertexBuffer;
	pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
	pipelineInfo.vertex_input_state.vertex_attributes = attributes.data();
	pipelineInfo.vertex_input_state.num_vertex_attributes = static_cast<Uint32>(attributes.size());
	pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
	pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
	pipelineInfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
	pipelineInfo.rasterizer_state.enable_depth_clip = true;
	pipelineInfo.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
	pipelineInfo.target_info.color_target_descriptions = &colorTarget;
	pipelineInfo.target_info.num_color_targets = 1;

	SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(rendererState.device, &pipelineInfo);
	if (!pipeline) {
		setShaderMessage(SDL_GetError());
		return nullptr;
	}

	pipelineCache.push_back(SDL3GPUPipelineEntry{targetFormat, useBlending, blendMode, vertexShader, fragmentShader, pipeline});
	return pipeline;
}

GPU_BlendMode normalBlendMode() {
	GPU_BlendMode mode{};
	mode.source_color   = GPU_FUNC_ONE;
	mode.dest_color     = GPU_FUNC_ONE_MINUS_SRC_ALPHA;
	mode.source_alpha   = GPU_FUNC_ONE;
	mode.dest_alpha     = GPU_FUNC_ONE_MINUS_SRC_ALPHA;
	mode.color_equation = GPU_EQ_ADD;
	mode.alpha_equation = GPU_EQ_ADD;
	return mode;
}

void setShaderMessage(const char *message) {
	std::snprintf(shaderMessage, sizeof(shaderMessage), "%s", message ? message : "");
}

bool containsText(const std::string &text, const char *needle) {
	return text.find(needle) != std::string::npos;
}

SDL3GPUShaderKind identifyShaderSource(GPU_ShaderEnum shaderType, const std::string &source) {
	if (shaderType == GPU_VERTEX_SHADER) {
		if (containsText(source, "gpu_ModelViewProjectionMatrix"))
			return SDL3GPUShaderKind::DefaultVertex;
		return SDL3GPUShaderKind::Unknown;
	}

	if (containsText(source, "constant_mask") && containsText(source, "crossfade"))
		return SDL3GPUShaderKind::BlendByMask;
	if (containsText(source, "NTEXTURES") && containsText(source, "subColors"))
		return SDL3GPUShaderKind::RenderSubtitles;
	if (containsText(source, "color.r *= color.a") && containsText(source, "color.g *= color.a") &&
	    !containsText(source, "modificationType"))
		return SDL3GPUShaderKind::MultiplyAlpha;
	if (containsText(source, "colorSrc") && containsText(source, "color.a = colorSrc.r"))
		return SDL3GPUShaderKind::MergeAlpha;
	if (containsText(source, "conversionType") && containsText(source, "YUVToRGB"))
		return SDL3GPUShaderKind::ColourConversion;
	if (containsText(source, "modificationType") && containsText(source, "replaceSrcColor"))
		return SDL3GPUShaderKind::ColorModification;
	if (containsText(source, "HORIZONTAL_BLUR_9"))
		return SDL3GPUShaderKind::BlurH;
	if (containsText(source, "VERTICAL_BLUR_9"))
		return SDL3GPUShaderKind::BlurV;
	if (containsText(source, "breakupCellforms"))
		return SDL3GPUShaderKind::Breakup;
	if (containsText(source, "uniform float alpha"))
		return SDL3GPUShaderKind::GlassSmash;
	if (containsText(source, "factor+1") && containsText(source, "cell_w"))
		return SDL3GPUShaderKind::Pixelate;
	if (containsText(source, "uniform int partial") && containsText(source, "uniform int full"))
		return SDL3GPUShaderKind::TextFade;
	if (containsText(source, "faceAscender") && containsText(source, "lightening_factor"))
		return SDL3GPUShaderKind::GlyphGradient;
	if (containsText(source, "script_width") && containsText(source, "TRVSWAVE_AMPLITUDE"))
		return SDL3GPUShaderKind::EffectTrvswave;
	if (containsText(source, "animationClock") && containsText(source, "wavelength"))
		return SDL3GPUShaderKind::EffectWarp;
	if (containsText(source, "effect_counter") && containsText(source, "OMEGA"))
		return SDL3GPUShaderKind::EffectWhirl;
	if (containsText(source, "fix crappy masks"))
		return SDL3GPUShaderKind::CropByMask;
	if (containsText(source, "insideBox") && !containsText(source, "uniform float alpha"))
		return SDL3GPUShaderKind::AlphaOutsideTextures;
	return SDL3GPUShaderKind::Unknown;
}

#if defined(ONS_USE_SDL3_SHADERCROSS)
std::string stripGLSLComments(const std::string &source) {
	std::string result;
	result.reserve(source.size());
	bool lineComment = false;
	bool blockComment = false;
	for (size_t i = 0; i < source.size(); ++i) {
		const char ch   = source[i];
		const char next = i + 1 < source.size() ? source[i + 1] : '\0';
		if (lineComment) {
			if (ch == '\n') {
				lineComment = false;
				result.push_back(ch);
			}
			continue;
		}
		if (blockComment) {
			if (ch == '*' && next == '/') {
				blockComment = false;
				++i;
			} else if (ch == '\n') {
				result.push_back(ch);
			}
			continue;
		}
		if (ch == '/' && next == '/') {
			lineComment = true;
			++i;
			continue;
		}
		if (ch == '/' && next == '*') {
			blockComment = true;
			++i;
			continue;
		}
		result.push_back(ch);
	}
	return result;
}

std::string regexReplace(const std::string &source, const char *pattern, const std::string &replacement) {
	return std::regex_replace(source, std::regex(pattern), replacement);
}

int parsePositiveInt(const std::string &value, const std::unordered_map<std::string, int> &defines, int fallback = 1) {
	if (value.empty())
		return fallback;
	char *end = nullptr;
	const long parsed = std::strtol(value.c_str(), &end, 10);
	if (end && *end == '\0' && parsed > 0)
		return static_cast<int>(parsed);
	auto it = defines.find(value);
	return it == defines.end() ? fallback : std::max(1, it->second);
}

int samplerSlotForName(const std::string &name, int fallback) {
	if (name == "tex" || name == "u_texture")
		return 0;
	if (name.size() > 3 && name.compare(0, 3, "tex") == 0) {
		char *end = nullptr;
		const long parsed = std::strtol(name.c_str() + 3, &end, 10);
		if (end && *end == '\0' && parsed >= 0 && parsed < 8)
			return static_cast<int>(parsed);
	}
	return fallback;
}

const char *hlslTypeName(const SDL3GPUNativeUniform &uniform) {
	if (uniform.type == SDL3GPUUniformType::Int)
		return "int";
	if (uniform.components <= 1)
		return "float";
	if (uniform.components == 2)
		return "float2";
	if (uniform.components == 3)
		return "float3";
	return "float4";
}

std::string hlslUniformMacroSuffix(const SDL3GPUNativeUniform &uniform) {
	if (uniform.type == SDL3GPUUniformType::Int)
		return ".x";
	if (uniform.components <= 1)
		return ".x";
	if (uniform.components == 2)
		return ".xy";
	if (uniform.components == 3)
		return ".xyz";
	return "";
}

bool parseLegacyGLSLUniforms(const std::string &source,
                             std::vector<SDL3GPUNativeUniform> &uniforms,
                             std::vector<std::pair<std::string, int>> &samplers,
                             Uint32 &samplerCount,
                             std::string &error) {
	std::unordered_map<std::string, int> defines;
	const std::regex defineRegex(R"(^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\s+([0-9]+)\s*$)");
	std::istringstream lines(source);
	std::string line;
	while (std::getline(lines, line)) {
		std::smatch match;
		if (std::regex_match(line, match, defineRegex))
			defines[match[1].str()] = parsePositiveInt(match[2].str(), defines);
	}

	const std::regex uniformRegex(
	    R"(\buniform\s+(?:(?:lowp|mediump|highp)\s+)?([A-Za-z_][A-Za-z0-9_]*)\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[\s*([A-Za-z_][A-Za-z0-9_]*|[0-9]+)\s*\])?\s*;)");
	int nextSamplerSlot = 0;
	Uint32 nextRegister = 0;
	for (auto it = std::sregex_iterator(source.begin(), source.end(), uniformRegex); it != std::sregex_iterator(); ++it) {
		const std::string type = (*it)[1].str();
		const std::string name = (*it)[2].str();
		const int arraySize    = parsePositiveInt((*it)[3].str(), defines);
		if (type == "sampler2D") {
			const int slot = samplerSlotForName(name, nextSamplerSlot);
			nextSamplerSlot = std::max(nextSamplerSlot, slot + 1);
			samplers.push_back(std::make_pair(name, slot));
			samplerCount = std::max<Uint32>(samplerCount, static_cast<Uint32>(slot + 1));
			continue;
		}

		SDL3GPUNativeUniform uniform{};
		uniform.name = name;
		uniform.arraySize = arraySize;
		uniform.registerIndex = nextRegister;
		if (type == "int" || type == "bool") {
			uniform.type = SDL3GPUUniformType::Int;
			uniform.components = 1;
		} else if (type == "float") {
			uniform.type = SDL3GPUUniformType::Float;
			uniform.components = 1;
		} else if (type == "vec2") {
			uniform.type = SDL3GPUUniformType::FloatVec;
			uniform.components = 2;
		} else if (type == "vec3") {
			uniform.type = SDL3GPUUniformType::FloatVec;
			uniform.components = 3;
		} else if (type == "vec4") {
			uniform.type = SDL3GPUUniformType::FloatVec;
			uniform.components = 4;
		} else {
			error = "Unsupported GLSL uniform type in SDL3 shadercross translator: " + type;
			return false;
		}
		nextRegister += static_cast<Uint32>(uniform.arraySize);
		uniforms.push_back(uniform);
	}
	return true;
}

bool sourceUsesVarying(const std::string &source, const char *name) {
	const std::regex varyingRegex(std::string(R"(\bvarying\s+(?:(?:lowp|mediump|highp)\s+)?[A-Za-z_][A-Za-z0-9_]*\s+)") +
	                             name + R"(\s*;)");
	return std::regex_search(source, varyingRegex);
}

bool translateLegacyGLSLFragmentToHLSL(const std::string &source,
                                       std::string &hlsl,
                                       std::vector<SDL3GPUNativeUniform> &uniforms,
                                       SDL3GPUNativeResourceInfo &resources,
                                       std::string &error) {
	const std::string cleaned = stripGLSLComments(source);
	std::vector<std::pair<std::string, int>> samplers;
	Uint32 samplerCount = 0;
	if (!parseLegacyGLSLUniforms(cleaned, uniforms, samplers, samplerCount, error))
		return false;

	std::ostringstream out;
	for (const auto &sampler : samplers) {
		out << "Texture2D " << sampler.first << "Texture : register(t" << sampler.second << ", space2);\n";
		out << "SamplerState " << sampler.first << "Sampler : register(s" << sampler.second << ", space2);\n";
	}

	if (!uniforms.empty()) {
		out << "cbuffer SDL3GPUCompatUniforms : register(b0, space3) {\n";
		for (const auto &uniform : uniforms) {
			out << "\t" << hlslTypeName(uniform) << " _ons_" << uniform.name;
			if (uniform.arraySize > 1)
				out << "[" << uniform.arraySize << "]";
			out << " : packoffset(c" << uniform.registerIndex << ");\n";
		}
		out << "};\n";
		for (const auto &uniform : uniforms) {
			out << "#define " << uniform.name << " _ons_" << uniform.name;
			if (uniform.arraySize <= 1)
				out << hlslUniformMacroSuffix(uniform);
			out << "\n";
		}
	}

	out << "struct PSInput { float4 color : TEXCOORD0; float2 texCoord : TEXCOORD1; };\n";
	out << "struct PSOutput { float4 o_color : SV_Target; };\n";

	std::string body = cleaned;
	body = regexReplace(body, R"(^\s*#\s*version[^\n]*(?:\n|$))", "");
	body = regexReplace(body, R"(^\s*precision\s+[A-Za-z_][A-Za-z0-9_]*\s+[A-Za-z_][A-Za-z0-9_]*\s*;\s*(?:\n|$))", "");
	body = regexReplace(body, R"(\buniform\s+(?:(?:lowp|mediump|highp)\s+)?[A-Za-z_][A-Za-z0-9_]*\s+[A-Za-z_][A-Za-z0-9_]*\s*(?:\[\s*(?:[A-Za-z_][A-Za-z0-9_]*|[0-9]+)\s*\])?\s*;)", "");
	body = regexReplace(body, R"(\bvarying\s+(?:(?:lowp|mediump|highp)\s+)?[A-Za-z_][A-Za-z0-9_]*\s+[A-Za-z_][A-Za-z0-9_]*\s*;)", "");
	body = regexReplace(body, R"(\btexture2D\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*,)", "$1Texture.Sample($1Sampler,");
	body = regexReplace(body, R"(\bgl_FragColor\b)", "output.o_color");
	const bool usesVaryingColor = sourceUsesVarying(cleaned, "color");
	const bool usesVaryingTexCoord = sourceUsesVarying(cleaned, "texCoord");
	if (usesVaryingColor)
		body = regexReplace(body, R"(\bcolor\b)", "input.color");
	if (usesVaryingTexCoord)
		body = regexReplace(body, R"(\btexCoord\b)", "input.texCoord");
	body = regexReplace(body, R"(\bvec2\b)", "float2");
	body = regexReplace(body, R"(\bvec3\b)", "float3");
	body = regexReplace(body, R"(\bvec4\b)", "float4");
	body = regexReplace(body, R"(\bmat4\b)", "float4x4");
	body = regexReplace(body, R"(\bmix\s*\()", "lerp(");
	body = regexReplace(body, R"(\bfract\s*\()", "frac(");
	body = regexReplace(body, R"(\bmod\s*\()", "fmod(");
	body = regexReplace(body, R"(\breturn\s*;)", "return output;");

	const std::regex mainRegex(R"(\bvoid\s+main\s*\(\s*(?:void)?\s*\)\s*\{)");
	if (!std::regex_search(body, mainRegex)) {
		error = "Legacy GLSL fragment shader has no void main() entrypoint";
		return false;
	}
	body = std::regex_replace(body, mainRegex,
	                          std::string("PSOutput main(PSInput input) {") +
	                              "\n\tPSOutput output;\n\toutput.o_color = float4(0.0, 0.0, 0.0, 0.0);");
	const size_t lastBrace = body.find_last_of('}');
	if (lastBrace == std::string::npos) {
		error = "Legacy GLSL fragment shader has no closing main() brace";
		return false;
	}
	body.insert(lastBrace, "\n\treturn output;\n");

	out << body << "\n";
	hlsl = out.str();
	resources.numSamplers = samplerCount;
	resources.numUniformBuffers = uniforms.empty() ? 0 : 1;
	return true;
}

bool looksLikeHLSL(const std::string &source) {
	return containsText(source, "SV_Target") || containsText(source, "SV_POSITION") ||
	       containsText(source, "Texture2D") || containsText(source, "SamplerState") ||
	       containsText(source, "cbuffer") || containsText(source, ": register(");
}

bool looksLikeLegacyGLSL(const std::string &source) {
	return containsText(source, "gl_FragColor") || containsText(source, "texture2D") ||
	       containsText(source, "varying");
}

bool looksLikeGLSL(const std::string &source) {
	return containsText(source, "#version") || containsText(source, "layout(") ||
	       containsText(source, "gl_Position") || containsText(source, "sampler2D") ||
	       containsText(source, "vec2") || containsText(source, "vec3") ||
	       containsText(source, "vec4");
}

bool looksLikeSPIRV(const std::string &source) {
	if (source.size() < 4)
		return false;
	const auto *bytes = reinterpret_cast<const Uint8 *>(source.data());
	const Uint32 magic = static_cast<Uint32>(bytes[0]) |
	                     (static_cast<Uint32>(bytes[1]) << 8) |
	                     (static_cast<Uint32>(bytes[2]) << 16) |
	                     (static_cast<Uint32>(bytes[3]) << 24);
	return magic == 0x07230203;
}

SDL3GPUNativeResourceInfo inferHLSLResourceInfo(const std::string &source) {
	SDL3GPUNativeResourceInfo resources{};
	const std::regex samplerRegisterRegex(R"(\bregister\s*\(\s*[ts]([0-9]+))");
	for (auto it = std::sregex_iterator(source.begin(), source.end(), samplerRegisterRegex); it != std::sregex_iterator(); ++it)
		resources.numSamplers = std::max<Uint32>(resources.numSamplers, static_cast<Uint32>(parsePositiveInt((*it)[1].str(), {}, 0) + 1));
	if (resources.numSamplers == 0) {
		const std::regex textureDeclRegex(R"(\bTexture2D(?:<[^>]+>)?\s+[A-Za-z_][A-Za-z0-9_]*)");
		resources.numSamplers = static_cast<Uint32>(std::distance(std::sregex_iterator(source.begin(), source.end(), textureDeclRegex),
		                                                           std::sregex_iterator()));
	}

	const std::regex uniformRegisterRegex(R"(\bregister\s*\(\s*b([0-9]+))");
	for (auto it = std::sregex_iterator(source.begin(), source.end(), uniformRegisterRegex); it != std::sregex_iterator(); ++it)
		resources.numUniformBuffers = std::max<Uint32>(resources.numUniformBuffers, static_cast<Uint32>(parsePositiveInt((*it)[1].str(), {}, 0) + 1));
	if (resources.numUniformBuffers == 0) {
		const std::regex cbufferRegex(R"(\bcbuffer\s+[A-Za-z_][A-Za-z0-9_]*)");
		resources.numUniformBuffers = static_cast<Uint32>(std::distance(std::sregex_iterator(source.begin(), source.end(), cbufferRegex),
		                                                                 std::sregex_iterator()));
	}
	return resources;
}

SDL_GPUShaderStage toSDLShaderStage(GPU_ShaderEnum shaderType) {
	return shaderType == GPU_VERTEX_SHADER ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
}

SDL_ShaderCross_ShaderStage toShaderCrossStage(GPU_ShaderEnum shaderType) {
	return shaderType == GPU_VERTEX_SHADER ? SDL_SHADERCROSS_SHADERSTAGE_VERTEX : SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
}

bool ensureShaderCross() {
	if (shaderCrossInitialized)
		return true;
	if (!SDL_ShaderCross_Init()) {
		setShaderMessage(SDL_GetError());
		return false;
	}
	shaderCrossInitialized = true;
	return true;
}

SDL_GPUShader *createNativeShaderFromBytecode(GPU_ShaderEnum shaderType,
                                              const void *bytecode,
                                              size_t bytecodeSize,
                                              SDL_GPUShaderFormat format,
                                              const SDL3GPUNativeResourceInfo &resources,
                                              const char *entrypoint = "main") {
	if (!rendererState.device || !bytecode || bytecodeSize == 0)
		return nullptr;

	SDL_GPUShaderCreateInfo shaderInfo{};
	shaderInfo.code_size = bytecodeSize;
	shaderInfo.code      = static_cast<const Uint8 *>(bytecode);
	shaderInfo.entrypoint = entrypoint;
	shaderInfo.format    = format;
	shaderInfo.stage     = toSDLShaderStage(shaderType);
	shaderInfo.num_samplers = resources.numSamplers;
	shaderInfo.num_storage_textures = resources.numStorageTextures;
	shaderInfo.num_storage_buffers = resources.numStorageBuffers;
	shaderInfo.num_uniform_buffers = resources.numUniformBuffers;
	return SDL_CreateGPUShader(rendererState.device, &shaderInfo);
}

SDL3GPUNativeResourceInfo toNativeResourceInfo(const SDL_ShaderCross_GraphicsShaderResourceInfo &info) {
	SDL3GPUNativeResourceInfo resources{};
	resources.numSamplers       = info.num_samplers;
	resources.numStorageTextures = info.num_storage_textures;
	resources.numStorageBuffers  = info.num_storage_buffers;
	resources.numUniformBuffers  = info.num_uniform_buffers;
	return resources;
}

#if defined(ONS_USE_SDL3_SHADERC)
shaderc_shader_kind toShadercKind(GPU_ShaderEnum shaderType) {
	return shaderType == GPU_VERTEX_SHADER ? shaderc_glsl_vertex_shader : shaderc_glsl_fragment_shader;
}

bool compileGLSLToSPIRV(GPU_ShaderEnum shaderType, const std::string &source, std::vector<Uint8> &spirv) {
	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	if (!compiler) {
		setShaderMessage("shaderc compiler initialization failed");
		return false;
	}

	shaderc_compile_options_t options = shaderc_compile_options_initialize();
	if (!options) {
		shaderc_compiler_release(compiler);
		setShaderMessage("shaderc compile options initialization failed");
		return false;
	}

	shaderc_compile_options_set_source_language(options, shaderc_source_language_glsl);
	shaderc_compile_options_set_target_env(options, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);
	shaderc_compile_options_set_target_spirv(options, shaderc_spirv_version_1_0);
	shaderc_compile_options_set_auto_bind_uniforms(options, true);
	shaderc_compile_options_set_auto_combined_image_sampler(options, true);
	shaderc_compile_options_set_auto_map_locations(options, true);
	shaderc_compile_options_set_vulkan_rules_relaxed(options, true);

	shaderc_compilation_result_t result = shaderc_compile_into_spv(compiler,
	                                                               source.data(),
	                                                               source.size(),
	                                                               toShadercKind(shaderType),
	                                                               shaderType == GPU_VERTEX_SHADER ? "ons.vert" : "ons.frag",
	                                                               "main",
	                                                               options);
	shaderc_compile_options_release(options);
	shaderc_compiler_release(compiler);

	if (!result) {
		setShaderMessage("shaderc returned no compilation result");
		return false;
	}

	const shaderc_compilation_status status = shaderc_result_get_compilation_status(result);
	if (status != shaderc_compilation_status_success) {
		const char *message = shaderc_result_get_error_message(result);
		setShaderMessage(message && *message ? message : "shaderc GLSL compilation failed");
		shaderc_result_release(result);
		return false;
	}

	const char *bytes = shaderc_result_get_bytes(result);
	const size_t length = shaderc_result_get_length(result);
	spirv.assign(reinterpret_cast<const Uint8 *>(bytes), reinterpret_cast<const Uint8 *>(bytes) + length);
	shaderc_result_release(result);
	return !spirv.empty();
}
#endif

SDL_GPUShader *compileNativeSPIRVShader(GPU_ShaderEnum shaderType,
                                        const Uint8 *bytecode,
                                        size_t bytecodeSize,
                                        SDL3GPUNativeResourceInfo &resources) {
	if (!ensureShaderCross())
		return nullptr;

	SDL_ShaderCross_GraphicsShaderMetadata *metadata = SDL_ShaderCross_ReflectGraphicsSPIRV(bytecode, bytecodeSize, 0);
	if (!metadata) {
		setShaderMessage(SDL_GetError());
		return nullptr;
	}

	SDL_ShaderCross_SPIRV_Info spirvInfo{};
	spirvInfo.bytecode = bytecode;
	spirvInfo.bytecode_size = bytecodeSize;
	spirvInfo.entrypoint = "main";
	spirvInfo.shader_stage = toShaderCrossStage(shaderType);
	spirvInfo.props = 0;

	SDL_GPUShader *shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(rendererState.device, &spirvInfo, &metadata->resource_info, 0);
	resources = toNativeResourceInfo(metadata->resource_info);
	SDL_free(metadata);
	if (!shader)
		setShaderMessage(SDL_GetError());
	return shader;
}

SDL_GPUShader *compileNativeGLSLShader(GPU_ShaderEnum shaderType,
                                       const std::string &glsl,
                                       SDL3GPUNativeResourceInfo &resources) {
#if defined(ONS_USE_SDL3_SHADERC)
	std::vector<Uint8> spirv;
	if (!compileGLSLToSPIRV(shaderType, glsl, spirv))
		return nullptr;
	return compileNativeSPIRVShader(shaderType, spirv.data(), spirv.size(), resources);
#else
	(void)shaderType;
	(void)glsl;
	(void)resources;
	setShaderMessage("External SDL3 GLSL shaders require shaderc or precompiled SPIR-V");
	return nullptr;
#endif
}

SDL_GPUShader *compileNativeHLSLShader(GPU_ShaderEnum shaderType,
                                       const std::string &hlsl,
                                       SDL3GPUNativeResourceInfo &resources) {
	if (!ensureShaderCross())
		return nullptr;

	SDL_ShaderCross_HLSL_Info hlslInfo{};
	hlslInfo.source = hlsl.c_str();
	hlslInfo.entrypoint = "main";
	hlslInfo.shader_stage = toShaderCrossStage(shaderType);
	hlslInfo.props = 0;

	size_t spirvSize = 0;
	void *spirv = SDL_ShaderCross_CompileSPIRVFromHLSL(&hlslInfo, &spirvSize);
	if (spirv) {
		SDL_GPUShader *shader = compileNativeSPIRVShader(shaderType, static_cast<const Uint8 *>(spirv), spirvSize, resources);
		SDL_free(spirv);
		if (shader)
			return shader;
	}

	const SDL_GPUShaderFormat supported = rendererState.device ? SDL_GetGPUShaderFormats(rendererState.device) : SDL_GPU_SHADERFORMAT_INVALID;
	SDL_PropertiesID props = SDL_CreateProperties();
	if (props)
		SDL_SetBooleanProperty(props, SDL_SHADERCROSS_PROP_HLSL_SKIP_SPIRV_ROUNDTRIP_BOOLEAN, true);
	hlslInfo.props = props;

	SDL_GPUShader *shader = nullptr;
	if (supported & SDL_GPU_SHADERFORMAT_DXIL) {
		size_t dxilSize = 0;
		void *dxil = SDL_ShaderCross_CompileDXILFromHLSL(&hlslInfo, &dxilSize);
		if (dxil) {
			shader = createNativeShaderFromBytecode(shaderType, dxil, dxilSize, SDL_GPU_SHADERFORMAT_DXIL, resources);
			SDL_free(dxil);
		}
	}
	if (!shader && (supported & SDL_GPU_SHADERFORMAT_DXBC)) {
		size_t dxbcSize = 0;
		void *dxbc = SDL_ShaderCross_CompileDXBCFromHLSL(&hlslInfo, &dxbcSize);
		if (dxbc) {
			shader = createNativeShaderFromBytecode(shaderType, dxbc, dxbcSize, SDL_GPU_SHADERFORMAT_DXBC, resources);
			SDL_free(dxbc);
		}
	}
	if (props)
		SDL_DestroyProperties(props);

	if (!shader)
		setShaderMessage(SDL_GetError());
	return shader;
}

bool compileNativeExternalShader(GPU_ShaderEnum shaderType, const std::string &source, SDL3GPUShaderObject &object) {
	if (!rendererState.device) {
		setShaderMessage("SDL3 shadercross compilation requires an initialized GPU device");
		return false;
	}

	if (looksLikeSPIRV(source)) {
		object.nativeShader = compileNativeSPIRVShader(shaderType,
		                                              reinterpret_cast<const Uint8 *>(source.data()),
		                                              source.size(),
		                                              object.nativeResources);
		return object.nativeShader != nullptr;
	}

	std::string hlsl;
	SDL3GPUNativeResourceInfo resources{};
	if (shaderType != GPU_VERTEX_SHADER && looksLikeLegacyGLSL(source)) {
		std::string error;
		if (!translateLegacyGLSLFragmentToHLSL(source, hlsl, object.nativeUniforms, resources, error)) {
			setShaderMessage(error.c_str());
			return false;
		}
		object.translatedLegacyGLSL = true;
	} else if (looksLikeHLSL(source)) {
		hlsl = source;
		resources = inferHLSLResourceInfo(hlsl);
	} else if (looksLikeGLSL(source)) {
		object.nativeShader = compileNativeGLSLShader(shaderType, source, object.nativeResources);
		return object.nativeShader != nullptr;
	} else {
		setShaderMessage("External SDL3 shader is neither SPIR-V, HLSL, GLSL, nor a supported legacy GLSL fragment");
		return false;
	}

	object.nativeResources = resources;
	object.nativeShader = compileNativeHLSLShader(shaderType, hlsl, object.nativeResources);
	return object.nativeShader != nullptr;
}
#else
bool compileNativeExternalShader(GPU_ShaderEnum, const std::string &, SDL3GPUShaderObject &) {
	setShaderMessage("SDL3_GPU backend only supports external shaders when built with SDL_shadercross");
	return false;
}
#endif

const char *shaderKindName(SDL3GPUShaderKind kind) {
	switch (kind) {
		case SDL3GPUShaderKind::DefaultVertex: return "default vertex";
		case SDL3GPUShaderKind::AlphaOutsideTextures: return "alphaOutsideTextures.frag";
		case SDL3GPUShaderKind::BlendByMask: return "blendByMask.frag";
		case SDL3GPUShaderKind::BlurH: return "blurH.frag";
		case SDL3GPUShaderKind::BlurV: return "blurV.frag";
		case SDL3GPUShaderKind::Breakup: return "breakup.frag";
		case SDL3GPUShaderKind::ColorModification: return "colorModification.frag";
		case SDL3GPUShaderKind::ColourConversion: return "colourConversion.frag";
		case SDL3GPUShaderKind::CropByMask: return "cropByMask.frag";
		case SDL3GPUShaderKind::EffectTrvswave: return "effectTrvswave.frag";
		case SDL3GPUShaderKind::EffectWarp: return "effectWarp.frag";
		case SDL3GPUShaderKind::EffectWhirl: return "effectWhirl.frag";
		case SDL3GPUShaderKind::GlassSmash: return "glassSmash.frag";
		case SDL3GPUShaderKind::GlyphGradient: return "glyphGradient.frag";
		case SDL3GPUShaderKind::MergeAlpha: return "mergeAlpha.frag";
		case SDL3GPUShaderKind::MultiplyAlpha: return "multiplyAlpha.frag";
		case SDL3GPUShaderKind::Pixelate: return "pixelate.frag";
		case SDL3GPUShaderKind::RenderSubtitles: return "renderSubtitles.frag";
		case SDL3GPUShaderKind::TextFade: return "textFade.frag";
		case SDL3GPUShaderKind::Unknown:
		default: return "unknown shader";
	}
}

SDL3GPUProgramObject *activeProgramObject() {
	if (!rendererState.current_context_target || !rendererState.current_context_target->context)
		return nullptr;
	const Uint32 program = rendererState.current_context_target->context->current_shader_program;
	if (program == 0)
		return nullptr;
	auto it = programObjects.find(program);
	return it == programObjects.end() ? nullptr : &it->second;
}

int uniformInt(const SDL3GPUProgramObject &program, const char *name, int fallback = 0) {
	auto it = program.uniforms.find(name);
	if (it == program.uniforms.end())
		return fallback;
	if (it->second.type == SDL3GPUUniformType::Int)
		return it->second.intValue;
	return static_cast<int>(it->second.values[0]);
}

float uniformFloat(const SDL3GPUProgramObject &program, const char *name, float fallback = 0.0f) {
	auto it = program.uniforms.find(name);
	if (it == program.uniforms.end())
		return fallback;
	if (it->second.type == SDL3GPUUniformType::Int)
		return static_cast<float>(it->second.intValue);
	return it->second.values[0];
}

SDL3GPUColorF uniformVec4(const SDL3GPUProgramObject &program, const char *name, SDL3GPUColorF fallback = {}) {
	auto it = program.uniforms.find(name);
	if (it == program.uniforms.end())
		return fallback;
	if (it->second.type == SDL3GPUUniformType::Int) {
		const float v = static_cast<float>(it->second.intValue);
		return SDL3GPUColorF{v, v, v, v};
	}
	return SDL3GPUColorF{it->second.values[0], it->second.values[1], it->second.values[2], it->second.values[3]};
}

Uint32 packFloatWord(float value) {
	Uint32 word = 0;
	static_assert(sizeof(word) == sizeof(value), "Unexpected float packing size");
	std::memcpy(&word, &value, sizeof(word));
	return word;
}

Uint32 packIntWord(int value) {
	Uint32 word = 0;
	static_assert(sizeof(word) == sizeof(value), "Unexpected int packing size");
	std::memcpy(&word, &value, sizeof(word));
	return word;
}

bool parseUniformArrayElement(const std::string &name, std::string &baseName, int &element) {
	const size_t open = name.find('[');
	const size_t close = name.find(']', open == std::string::npos ? 0 : open);
	if (open == std::string::npos || close == std::string::npos || close <= open + 1)
		return false;
	baseName = name.substr(0, open);
	const std::string index = name.substr(open + 1, close - open - 1);
	char *end = nullptr;
	const long parsed = std::strtol(index.c_str(), &end, 10);
	if (!end || *end != '\0' || parsed < 0)
		return false;
	element = static_cast<int>(parsed);
	return true;
}

const SDL3GPUNativeUniform *nativeUniformForName(const SDL3GPUProgramObject &program,
                                                const std::string &name,
                                                Uint32 &registerIndex) {
	auto it = program.nativeUniformLookup.find(name);
	std::string baseName;
	int element = 0;
	if (it == program.nativeUniformLookup.end() && parseUniformArrayElement(name, baseName, element))
		it = program.nativeUniformLookup.find(baseName);
	if (it == program.nativeUniformLookup.end())
		return nullptr;

	const SDL3GPUNativeUniform &uniform = program.nativeUniforms[it->second];
	if (parseUniformArrayElement(name, baseName, element)) {
		if (baseName != uniform.name || element >= uniform.arraySize)
			return nullptr;
	} else {
		element = 0;
	}
	registerIndex = uniform.registerIndex + static_cast<Uint32>(element);
	if (registerIndex >= program.nativeUniformRegisters.size())
		return nullptr;
	return &uniform;
}

void updateNativeUniformRegister(SDL3GPUProgramObject &program,
                                 const std::string &name,
                                 const SDL3GPUUniformValue &value) {
	Uint32 registerIndex = 0;
	const SDL3GPUNativeUniform *uniform = nativeUniformForName(program, name, registerIndex);
	if (!uniform)
		return;

	SDL3GPUNativeUniformRegister &reg = program.nativeUniformRegisters[registerIndex];
	for (auto &word : reg.words)
		word = 0;

	const int components = std::max(1, std::min(uniform->components, value.components));
	for (int i = 0; i < components; ++i) {
		if (uniform->type == SDL3GPUUniformType::Int) {
			const int intValue = value.type == SDL3GPUUniformType::Int ? value.intValue : static_cast<int>(value.values[i]);
			reg.words[i] = packIntWord(intValue);
		} else {
			const float floatValue = value.type == SDL3GPUUniformType::Int ? static_cast<float>(value.intValue) : value.values[i];
			reg.words[i] = packFloatWord(floatValue);
		}
	}
}

std::string indexedUniformName(const char *base, int index) {
	return std::string(base) + "[" + std::to_string(index) + "]";
}

float clampFloat(float value, float low = 0.0f, float high = 1.0f) {
	return std::max(low, std::min(high, value));
}

SDL3GPUColorF clampColor(SDL3GPUColorF color) {
	color.r = clampFloat(color.r);
	color.g = clampFloat(color.g);
	color.b = clampFloat(color.b);
	color.a = clampFloat(color.a);
	return color;
}

SDL3GPUColorF addColor(SDL3GPUColorF a, SDL3GPUColorF b) {
	return SDL3GPUColorF{a.r + b.r, a.g + b.g, a.b + b.b, a.a + b.a};
}

SDL3GPUColorF mulColor(SDL3GPUColorF color, float scalar) {
	return SDL3GPUColorF{color.r * scalar, color.g * scalar, color.b * scalar, color.a * scalar};
}

SDL3GPUColorF yuvToRgb(SDL3GPUColorF yuv) {
	const float y  = (yuv.r - 16.0f / 255.0f) * 1.16438f;
	const float cb = yuv.g - 128.0f / 255.0f;
	const float cr = yuv.b - 128.0f / 255.0f;
	return SDL3GPUColorF{
	    y + cr * 1.79274f,
	    y - 0.532910f * cr - 0.213250f * cb,
	    y + cb * 2.11240f,
	    1.0f};
}

void setUnsupported(const char *functionName) {
	SDL_SetError("%s is not implemented in the SDL3_GPU transition backend", functionName);
	setShaderMessage(SDL_GetError());
}

void initialiseImageDefaults(GPU_Image *image, Uint16 w, Uint16 h, GPU_FormatEnum format) {
	image->w                   = w;
	image->h                   = h;
	image->base_w              = w;
	image->base_h              = h;
	image->texture_w           = w;
	image->texture_h           = h;
	image->format              = format;
	image->bytes_per_pixel     = textureBytesPerPixel(format);
	image->pitch               = image->bytes_per_pixel * w;
	image->color               = SDL_Color{255, 255, 255, 255};
	image->use_blending        = true;
	image->blend_mode          = normalBlendMode();
	image->filter_mode         = GPU_FILTER_LINEAR;
	image->snap_mode           = GPU_SNAP_POSITION_AND_DIMENSIONS;
	image->anchor_x            = 0.5f;
	image->anchor_y            = 0.5f;
	image->refcount            = 1;
	image->renderer            = rendererState.device ? &rendererState : nullptr;
	image->context_target      = rendererState.current_context_target;
	image->pixels.resize(static_cast<size_t>(image->pitch) * h);
	image->pixels_dirty        = false;
}

bool createTexture(GPU_Image *image) {
	if (!image || !rendererState.device)
		return false;

	SDL_GPUTextureCreateInfo textureInfo{};
	textureInfo.type                 = SDL_GPU_TEXTURETYPE_2D;
	textureInfo.format               = textureFormat(image->format);
	textureInfo.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
	textureInfo.width                = image->w;
	textureInfo.height               = image->h;
	textureInfo.layer_count_or_depth = 1;
	textureInfo.num_levels           = 1;
	textureInfo.sample_count         = SDL_GPU_SAMPLECOUNT_1;

	image->texture = SDL_CreateGPUTexture(rendererState.device, &textureInfo);
	return image->texture != nullptr;
}

bool uploadImage(GPU_Image *image) {
	if (!image || !image->texture || !rendererState.device || image->pixels.empty())
		return false;

	SDL_GPUTransferBufferCreateInfo transferInfo{};
	transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	transferInfo.size  = static_cast<Uint32>(image->pixels.size());

	SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(rendererState.device, &transferInfo);
	if (!transfer)
		return false;

	void *mapped = SDL_MapGPUTransferBuffer(rendererState.device, transfer, false);
	if (!mapped) {
		SDL_ReleaseGPUTransferBuffer(rendererState.device, transfer);
		return false;
	}

	std::memcpy(mapped, image->pixels.data(), image->pixels.size());
	SDL_UnmapGPUTransferBuffer(rendererState.device, transfer);

	SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(rendererState.device);
	if (!commands) {
		SDL_ReleaseGPUTransferBuffer(rendererState.device, transfer);
		return false;
	}

	SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(commands);
	SDL_GPUTextureTransferInfo source{};
	source.transfer_buffer = transfer;
	source.pixels_per_row  = image->w;
	source.rows_per_layer  = image->h;

	SDL_GPUTextureRegion destination{};
	destination.texture = image->texture;
	destination.w       = image->w;
	destination.h       = image->h;
	destination.d       = 1;

	SDL_UploadToGPUTexture(copyPass, &source, &destination, true);
	SDL_EndGPUCopyPass(copyPass);
	const bool submitted = SDL_SubmitGPUCommandBuffer(commands);
	SDL_ReleaseGPUTransferBuffer(rendererState.device, transfer);
	if (submitted)
		image->pixels_dirty = false;
	return submitted;
}

bool downloadImage(GPU_Image *image) {
	if (!image || !image->texture || !rendererState.device || image->pixels.empty())
		return false;
	if (!image->pixels_dirty)
		return true;

	SDL_GPUTransferBufferCreateInfo transferInfo{};
	transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
	transferInfo.size  = static_cast<Uint32>(image->pixels.size());
	SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(rendererState.device, &transferInfo);
	if (!transfer)
		return false;

	SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(rendererState.device);
	if (!commands) {
		SDL_ReleaseGPUTransferBuffer(rendererState.device, transfer);
		return false;
	}

	SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(commands);
	SDL_GPUTextureRegion source{};
	source.texture = image->texture;
	source.w       = image->w;
	source.h       = image->h;
	source.d       = 1;

	SDL_GPUTextureTransferInfo destination{};
	destination.transfer_buffer = transfer;
	destination.pixels_per_row  = image->w;
	destination.rows_per_layer  = image->h;
	SDL_DownloadFromGPUTexture(copyPass, &source, &destination);
	SDL_EndGPUCopyPass(copyPass);

	SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
	if (!fence) {
		SDL_ReleaseGPUTransferBuffer(rendererState.device, transfer);
		return false;
	}

	const bool waited = SDL_WaitForGPUFences(rendererState.device, true, &fence, 1);
	SDL_ReleaseGPUFence(rendererState.device, fence);
	if (!waited) {
		SDL_ReleaseGPUTransferBuffer(rendererState.device, transfer);
		return false;
	}

	void *mapped = SDL_MapGPUTransferBuffer(rendererState.device, transfer, false);
	if (!mapped) {
		SDL_ReleaseGPUTransferBuffer(rendererState.device, transfer);
		return false;
	}
	std::memcpy(image->pixels.data(), mapped, image->pixels.size());
	SDL_UnmapGPUTransferBuffer(rendererState.device, transfer);
	SDL_ReleaseGPUTransferBuffer(rendererState.device, transfer);
	image->pixels_dirty = false;
	return true;
}

void ensureImagePixelsCurrent(GPU_Image *image) {
	if (image && image->pixels_dirty)
		downloadImage(image);
}

void copyPixelRow(Uint8 *dst, int dstBpp, const Uint8 *src, int srcBpp, int width) {
	if (dstBpp == srcBpp) {
		std::memcpy(dst, src, static_cast<size_t>(width) * dstBpp);
		return;
	}

	for (int x = 0; x < width; ++x) {
		const Uint8 *srcPixel = src + x * srcBpp;
		Uint8 *dstPixel       = dst + x * dstBpp;

		if (dstBpp == 4) {
			dstPixel[0] = srcBpp > 0 ? srcPixel[0] : 0;
			dstPixel[1] = srcBpp > 1 ? srcPixel[1] : dstPixel[0];
			dstPixel[2] = srcBpp > 2 ? srcPixel[2] : dstPixel[0];
			dstPixel[3] = srcBpp > 3 ? srcPixel[3] : 255;
		} else if (dstBpp == 2) {
			dstPixel[0] = srcBpp > 0 ? srcPixel[0] : 0;
			dstPixel[1] = srcBpp > 3 ? srcPixel[3] : 255;
		} else if (dstBpp == 1) {
			dstPixel[0] = srcBpp > 0 ? srcPixel[0] : 0;
		}
	}
}

bool extensionIsBmp(const char *filename) {
	if (!filename)
		return false;
	const char *extension = std::strrchr(filename, '.');
	if (!extension)
		return false;
	return std::tolower(static_cast<unsigned char>(extension[1])) == 'b' &&
	       std::tolower(static_cast<unsigned char>(extension[2])) == 'm' &&
	       std::tolower(static_cast<unsigned char>(extension[3])) == 'p' &&
	       extension[4] == '\0';
}

bool extensionIsPng(const char *filename) {
	if (!filename)
		return false;
	const char *extension = std::strrchr(filename, '.');
	if (!extension)
		return false;
	return std::tolower(static_cast<unsigned char>(extension[1])) == 'p' &&
	       std::tolower(static_cast<unsigned char>(extension[2])) == 'n' &&
	       std::tolower(static_cast<unsigned char>(extension[3])) == 'g' &&
	       extension[4] == '\0';
}

void pngWriteData(png_structp png_ptr, png_bytep data, png_size_t length) {
	SDL_RWops *rwops = static_cast<SDL_RWops *>(png_get_io_ptr(png_ptr));
	if (!rwops || onsRWwrite(rwops, data, 1, length) != length)
		png_error(png_ptr, "Failed to write PNG data");
}

void pngFlushData(png_structp) {}

bool saveSurfacePNG_RW(SDL_Surface *surface, SDL_RWops *rwops, bool free_rwops) {
	if (!surface || !rwops)
		return false;

	SDL_Surface *rgba = surface;
	bool freeSurface = false;
	if (onsSurfaceBytesPerPixel(surface) != 4) {
		rgba = onsConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA8888, SDL_SWSURFACE);
		freeSurface = rgba != nullptr;
	}
	if (!rgba) {
		if (free_rwops)
			SDL_RWclose(rwops);
		return false;
	}

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	png_infop info_ptr = png_ptr ? png_create_info_struct(png_ptr) : nullptr;
	bool saved = false;
	if (!png_ptr || !info_ptr)
		goto done;

	if (setjmp(png_jmpbuf(png_ptr)))
		goto done;

	png_set_write_fn(png_ptr, rwops, pngWriteData, pngFlushData);
	png_set_IHDR(png_ptr, info_ptr, rgba->w, rgba->h, 8, PNG_COLOR_TYPE_RGBA,
	             PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png_ptr, info_ptr);

	if (SDL_MUSTLOCK(rgba) && !SDL_LockSurface(rgba))
		goto done;
	{
		std::vector<png_bytep> rows(static_cast<size_t>(rgba->h));
		for (int y = 0; y < rgba->h; ++y)
			rows[y] = static_cast<png_bytep>(rgba->pixels) + y * rgba->pitch;
		png_write_image(png_ptr, rows.data());
	}
	if (SDL_MUSTLOCK(rgba))
		SDL_UnlockSurface(rgba);

	png_write_end(png_ptr, info_ptr);
	saved = true;

done:
	if (png_ptr || info_ptr)
		png_destroy_write_struct(png_ptr ? &png_ptr : nullptr, info_ptr ? &info_ptr : nullptr);
	if (freeSurface)
		SDL_FreeSurface(rgba);
	if (free_rwops)
		SDL_RWclose(rwops);
	return saved;
}

void releaseImageTexture(GPU_Image *image) {
	if (!image || !image->texture || !rendererState.device)
		return;
	SDL_ReleaseGPUTexture(rendererState.device, image->texture);
	image->texture = nullptr;
}

void releaseNativeRendererObjects() {
	if (!rendererState.device)
		return;
	for (auto &entry : pipelineCache) {
		if (entry.pipeline)
			SDL_ReleaseGPUGraphicsPipeline(rendererState.device, entry.pipeline);
	}
	pipelineCache.clear();
	if (nearestSampler) {
		SDL_ReleaseGPUSampler(rendererState.device, nearestSampler);
		nearestSampler = nullptr;
	}
	if (linearSampler) {
		SDL_ReleaseGPUSampler(rendererState.device, linearSampler);
		linearSampler = nullptr;
	}
	if (texturedVertexShader) {
		SDL_ReleaseGPUShader(rendererState.device, texturedVertexShader);
		texturedVertexShader = nullptr;
	}
	if (texturedFragmentShader) {
		SDL_ReleaseGPUShader(rendererState.device, texturedFragmentShader);
		texturedFragmentShader = nullptr;
	}
	for (auto &entry : shaderObjects) {
		if (entry.second.nativeShader)
			SDL_ReleaseGPUShader(rendererState.device, entry.second.nativeShader);
	}
	shaderObjects.clear();
	programObjects.clear();
	uniformLocationOwners.clear();
	nextUniformLocation = 1;
	nextShaderObject = 1;
#if defined(ONS_USE_SDL3_SHADERCROSS)
	if (shaderCrossInitialized) {
		SDL_ShaderCross_Quit();
		shaderCrossInitialized = false;
	}
#endif
}

bool resizeTargetBacking(GPU_Target *target, Uint16 w, Uint16 h) {
	if (!target || !target->is_window)
		return false;

	if (!target->image) {
		target->image          = new GPU_Image{};
		target->image->target  = nullptr;
		target->image->context_target = target;
	}

	releaseImageTexture(target->image);
	target->texture = nullptr;
	initialiseImageDefaults(target->image, w, h, GPU_FORMAT_RGBA);
	target->image->renderer       = &rendererState;
	target->image->context_target = target;
	target->image->target         = nullptr;
	if (!createTexture(target->image))
		return false;

	target->texture = target->image->texture;
	return uploadImage(target->image);
}

bool ensureTargetBacking(GPU_Target *target) {
	if (!target)
		return false;
	if (target->image)
		return true;
	if (!target->is_window)
		return false;
	return resizeTargetBacking(target, target->w, target->h);
}

Uint8 clampByte(int value) {
	return static_cast<Uint8>(std::max(0, std::min(255, value)));
}

SDL_Color readImagePixel(const GPU_Image *image, int x, int y) {
	SDL_Color color{0, 0, 0, 0};
	if (!image || x < 0 || y < 0 || x >= image->w || y >= image->h)
		return color;

	const Uint8 *pixel = image->pixels.data() + y * image->pitch + x * image->bytes_per_pixel;
	if (image->bytes_per_pixel == 1) {
		color = SDL_Color{pixel[0], pixel[0], pixel[0], 255};
	} else if (image->bytes_per_pixel == 2) {
		color = SDL_Color{pixel[0], pixel[0], pixel[0], pixel[1]};
	} else {
		color = SDL_Color{pixel[0], pixel[1], pixel[2], static_cast<Uint8>(image->format == GPU_FORMAT_RGB ? 255 : pixel[3])};
	}
	return color;
}

SDL_Color readImagePixelFromData(const GPU_Image *image, const Uint8 *pixels, int x, int y) {
	SDL_Color color{0, 0, 0, 0};
	if (!image || !pixels || x < 0 || y < 0 || x >= image->w || y >= image->h)
		return color;

	const Uint8 *pixel = pixels + y * image->pitch + x * image->bytes_per_pixel;
	if (image->bytes_per_pixel == 1) {
		color = SDL_Color{pixel[0], pixel[0], pixel[0], 255};
	} else if (image->bytes_per_pixel == 2) {
		color = SDL_Color{pixel[0], pixel[0], pixel[0], pixel[1]};
	} else {
		color = SDL_Color{pixel[0], pixel[1], pixel[2], static_cast<Uint8>(image->format == GPU_FORMAT_RGB ? 255 : pixel[3])};
	}
	return color;
}

SDL3GPUColorF toColorF(SDL_Color color) {
	return SDL3GPUColorF{
	    color.r / 255.0f,
	    color.g / 255.0f,
	    color.b / 255.0f,
	    color.a / 255.0f};
}

SDL_Color toSDLColor(SDL3GPUColorF color) {
	color = clampColor(color);
	return SDL_Color{
	    static_cast<Uint8>(std::lround(color.r * 255.0f)),
	    static_cast<Uint8>(std::lround(color.g * 255.0f)),
	    static_cast<Uint8>(std::lround(color.b * 255.0f)),
	    static_cast<Uint8>(std::lround(color.a * 255.0f))};
}

struct SDL3GPUTextureView {
	GPU_Image *image{nullptr};
	const Uint8 *pixels{nullptr};
};

SDL3GPUColorF sampleTexture(const SDL3GPUTextureView &view, float u, float v) {
	if (!view.image || !view.pixels || view.image->w <= 0 || view.image->h <= 0)
		return SDL3GPUColorF{};

	u = clampFloat(u);
	v = clampFloat(v);
	int x = static_cast<int>(u * view.image->w);
	int y = static_cast<int>(v * view.image->h);
	if (x >= view.image->w)
		x = view.image->w - 1;
	if (y >= view.image->h)
		y = view.image->h - 1;
	return toColorF(readImagePixelFromData(view.image, view.pixels, x, y));
}

SDL3GPUColorF sampleSlot(const std::array<SDL3GPUTextureView, 8> &textures, int slot, float u, float v) {
	if (slot < 0 || slot >= static_cast<int>(textures.size()))
		return SDL3GPUColorF{};
	return sampleTexture(textures[static_cast<size_t>(slot)], u, v);
}

bool insideUnitBox(float u, float v) {
	return u >= 0.0f && v >= 0.0f && u < 1.0f && v < 1.0f;
}

void writeImagePixel(GPU_Image *image, int x, int y, SDL_Color color) {
	if (!image || x < 0 || y < 0 || x >= image->w || y >= image->h)
		return;

	Uint8 *pixel = image->pixels.data() + y * image->pitch + x * image->bytes_per_pixel;
	if (image->bytes_per_pixel == 1) {
		pixel[0] = static_cast<Uint8>((static_cast<int>(color.r) + color.g + color.b) / 3);
	} else if (image->bytes_per_pixel == 2) {
		pixel[0] = static_cast<Uint8>((static_cast<int>(color.r) + color.g + color.b) / 3);
		pixel[1] = color.a;
	} else {
		pixel[0] = color.r;
		pixel[1] = color.g;
		pixel[2] = color.b;
		pixel[3] = color.a;
	}
}

SDL_Color modulatePixel(SDL_Color color, const GPU_Image *image) {
	if (!image)
		return color;
	color.r = static_cast<Uint8>((static_cast<int>(color.r) * image->color.r + 127) / 255);
	color.g = static_cast<Uint8>((static_cast<int>(color.g) * image->color.g + 127) / 255);
	color.b = static_cast<Uint8>((static_cast<int>(color.b) * image->color.b + 127) / 255);
	color.a = static_cast<Uint8>((static_cast<int>(color.a) * image->color.a + 127) / 255);
	return color;
}

bool usesStraightAlphaBlend(const GPU_Image *image) {
	return image && image->blend_mode.source_color == GPU_FUNC_SRC_ALPHA;
}

SDL_Color blendPixel(const GPU_Image *sourceImage, SDL_Color source, SDL_Color destination) {
	if (!sourceImage || !sourceImage->use_blending)
		return source;

	if (sourceImage->blend_mode.color_equation == GPU_EQ_SUBTRACT) {
		return SDL_Color{
		    clampByte(static_cast<int>(destination.r) - source.r),
		    clampByte(static_cast<int>(destination.g) - source.g),
		    clampByte(static_cast<int>(destination.b) - source.b),
		    destination.a};
	}

	if (sourceImage->blend_mode.source_color == GPU_FUNC_DST_COLOR &&
	    sourceImage->blend_mode.dest_color == GPU_FUNC_ZERO) {
		return SDL_Color{
		    static_cast<Uint8>((static_cast<int>(source.r) * destination.r + 127) / 255),
		    static_cast<Uint8>((static_cast<int>(source.g) * destination.g + 127) / 255),
		    static_cast<Uint8>((static_cast<int>(source.b) * destination.b + 127) / 255),
		    source.a};
	}

	if (sourceImage->blend_mode.dest_color == GPU_FUNC_ONE) {
		return SDL_Color{
		    clampByte(static_cast<int>(destination.r) + source.r),
		    clampByte(static_cast<int>(destination.g) + source.g),
		    clampByte(static_cast<int>(destination.b) + source.b),
		    clampByte(static_cast<int>(destination.a) + source.a)};
	}

	const int inverseAlpha = 255 - source.a;
	const bool straightAlpha = usesStraightAlphaBlend(sourceImage);
	const int sourceR = straightAlpha ? (static_cast<int>(source.r) * source.a + 127) / 255 : source.r;
	const int sourceG = straightAlpha ? (static_cast<int>(source.g) * source.a + 127) / 255 : source.g;
	const int sourceB = straightAlpha ? (static_cast<int>(source.b) * source.a + 127) / 255 : source.b;

	return SDL_Color{
	    clampByte(sourceR + (static_cast<int>(destination.r) * inverseAlpha + 127) / 255),
	    clampByte(sourceG + (static_cast<int>(destination.g) * inverseAlpha + 127) / 255),
	    clampByte(sourceB + (static_cast<int>(destination.b) * inverseAlpha + 127) / 255),
	    clampByte(static_cast<int>(source.a) + (static_cast<int>(destination.a) * inverseAlpha + 127) / 255)};
}

bool targetAllowsPixel(const GPU_Target *target, int x, int y) {
	if (!target || !target->image || x < 0 || y < 0 || x >= target->image->w || y >= target->image->h)
		return false;
	if (!target->use_clip_rect)
		return true;
	return x >= static_cast<int>(target->clip_rect.x) &&
	       y >= static_cast<int>(target->clip_rect.y) &&
	       x < static_cast<int>(target->clip_rect.x + target->clip_rect.w) &&
	       y < static_cast<int>(target->clip_rect.y + target->clip_rect.h);
}

SDL3GPUColorF evaluateColorModification(const SDL3GPUProgramObject &program,
                                        const std::array<SDL3GPUTextureView, 8> &textures,
                                        float u, float v) {
	SDL3GPUColorF color{};
	float grey = 0.0f;
	const int modificationType = uniformInt(program, "modificationType");
	const int dimension = std::max(1, uniformInt(program, "dimension", textures[0].image ? textures[0].image->w : 1));
	const float blurSize = 1.0f / static_cast<float>(dimension);

	if (modificationType == 0) {
		color = sampleSlot(textures, 0, u, v);
	} else if (modificationType == 1) {
		color = sampleSlot(textures, 0, u, v);
		grey = color.r * 0.299f + color.g * 0.587f + color.b * 0.114f;
		if (color.a != 0.0f)
			color = SDL3GPUColorF{grey * 1.2f, grey, grey * 0.8f, color.a};
	} else if (modificationType == 2 || modificationType == 3) {
		static const float weights[9]{0.05f, 0.09f, 0.12f, 0.15f, 0.16f, 0.15f, 0.12f, 0.09f, 0.05f};
		for (int i = -4; i <= 4; ++i) {
			const float du = modificationType == 2 ? i * blurSize : 0.0f;
			const float dv = modificationType == 3 ? i * blurSize : 0.0f;
			color = addColor(color, mulColor(sampleSlot(textures, 0, u + du, v + dv), weights[i + 4]));
		}
	} else if (modificationType == 4) {
		color = sampleSlot(textures, 0, u, v);
		grey = color.r * 0.299f + color.g * 0.587f + color.b * 0.114f;
		if (color.a != 0.0f)
			color = SDL3GPUColorF{grey, grey, grey, color.a};
	} else if (modificationType == 5) {
		color = sampleSlot(textures, 0, u, v);
		if (color.a != 0.0f)
			color = SDL3GPUColorF{1.0f - color.r, 1.0f - color.g, 1.0f - color.b, color.a};
	} else if (modificationType == 6) {
		color = sampleSlot(textures, 0, u, v);
		const SDL3GPUColorF darkenHue = uniformVec4(program, "darkenHue", SDL3GPUColorF{1.0f, 1.0f, 1.0f, 1.0f});
		if (color.a != 0.0f)
			color = SDL3GPUColorF{color.r * darkenHue.r, color.g * darkenHue.g, color.b * darkenHue.b, color.a};
	} else if (modificationType == 7) {
		color = sampleSlot(textures, 0, u, v);
		const SDL3GPUColorF src = uniformVec4(program, "replaceSrcColor");
		const SDL3GPUColorF dst = uniformVec4(program, "replaceDstColor");
		const float epsilon = 0.5f / 255.0f;
		if (std::fabs(color.r - src.r) <= epsilon &&
		    std::fabs(color.g - src.g) <= epsilon &&
		    std::fabs(color.b - src.b) <= epsilon) {
			color.r = dst.r;
			color.g = dst.g;
			color.b = dst.b;
		}
	}

	if (modificationType == 0 || uniformInt(program, "multiplyAlpha") == 1) {
		color.r *= color.a;
		color.g *= color.a;
		color.b *= color.a;
	}
	return color;
}

SDL3GPUColorF evaluateGaussianBlur(const SDL3GPUProgramObject &program,
                                   const std::array<SDL3GPUTextureView, 8> &textures,
                                   float u, float v, bool vertical) {
	const float sigma = std::max(uniformFloat(program, "sigma", 1.0f), 0.0001f);
	const float blurSize = uniformFloat(program, "blurSize", 0.0f);
	constexpr float pi = 3.14159265358979323846f;

	float incremental[3]{
	    1.0f / (std::sqrt(2.0f * pi) * sigma),
	    std::exp(-0.5f / (sigma * sigma)),
	    0.0f};
	incremental[2] = incremental[1] * incremental[1];

	SDL3GPUColorF avg = mulColor(sampleSlot(textures, 0, u, v), incremental[0]);
	float coefficientSum = incremental[0];
	incremental[0] *= incremental[1];
	incremental[1] *= incremental[2];

	for (float i = 1.0f; i <= 4.0f; i += 1.0f) {
		const float du = vertical ? 0.0f : i * blurSize;
		const float dv = vertical ? i * blurSize : 0.0f;
		avg = addColor(avg, mulColor(sampleSlot(textures, 0, u - du, v - dv), incremental[0]));
		avg = addColor(avg, mulColor(sampleSlot(textures, 0, u + du, v + dv), incremental[0]));
		coefficientSum += 2.0f * incremental[0];
		incremental[0] *= incremental[1];
		incremental[1] *= incremental[2];
	}

	return coefficientSum > 0.0f ? mulColor(avg, 1.0f / coefficientSum) : avg;
}

SDL3GPUColorF evaluateColourConversion(const SDL3GPUProgramObject &program,
                                       const std::array<SDL3GPUTextureView, 8> &textures,
                                       float u, float v) {
	const int conversionType = uniformInt(program, "conversionType");
	const int maskHeight = uniformInt(program, "maskHeight");
	if (maskHeight > 0 && v > 0.5f)
		return SDL3GPUColorF{};

	auto grabYUV = [&](float sampleU, float sampleV) {
		if (conversionType == 1) {
			return SDL3GPUColorF{
			    sampleSlot(textures, 0, sampleU, sampleV).r,
			    sampleSlot(textures, 1, sampleU, sampleV).r,
			    sampleSlot(textures, 2, sampleU, sampleV).r,
			    1.0f};
		}
		const SDL3GPUColorF uv = sampleSlot(textures, 1, sampleU, sampleV);
		return SDL3GPUColorF{sampleSlot(textures, 0, sampleU, sampleV).r, uv.r, uv.a, 1.0f};
	};

	SDL3GPUColorF rgba = yuvToRgb(grabYUV(u, v));
	rgba.a = 1.0f;
	if (maskHeight > 0) {
		rgba.a = yuvToRgb(grabYUV(u, v + 0.5f)).r;
		rgba.r *= rgba.a;
		rgba.g *= rgba.a;
		rgba.b *= rgba.a;
	}
	return rgba;
}

SDL3GPUColorF evaluateShaderPixel(const SDL3GPUProgramObject &program,
                                  const std::array<SDL3GPUTextureView, 8> &textures,
                                  float u, float v) {
	switch (program.kind) {
		case SDL3GPUShaderKind::AlphaOutsideTextures:
			return insideUnitBox(u, v) ? sampleSlot(textures, 0, u, v) : SDL3GPUColorF{};

		case SDL3GPUShaderKind::BlendByMask: {
			const SDL3GPUColorF img1 = sampleSlot(textures, 0, u, v);
			const SDL3GPUColorF img2 = sampleSlot(textures, 1, u, v);
			const SDL3GPUColorF mask = uniformInt(program, "constant_mask") ? SDL3GPUColorF{} : sampleSlot(textures, 2, u, v);
			const int maskValue = uniformInt(program, "mask_value");
			if (uniformInt(program, "crossfade")) {
				const float left = clampFloat((256.0f + mask.r * 256.0f - static_cast<float>(maskValue)) / 256.0f);
				return clampColor(addColor(mulColor(img1, left), mulColor(img2, 1.0f - left)));
			}
			return mask.r * 256.0f >= static_cast<float>(maskValue) ? img1 : img2;
		}

		case SDL3GPUShaderKind::BlurH:
			return evaluateGaussianBlur(program, textures, u, v, false);

		case SDL3GPUShaderKind::BlurV:
			return evaluateGaussianBlur(program, textures, u, v, true);

		case SDL3GPUShaderKind::Breakup: {
			const float tilesX = std::max(uniformFloat(program, "tilesX", 1.0f), 1.0f);
			const float tilesY = std::max(uniformFloat(program, "tilesY", 1.0f), 1.0f);
			const int breakupCellforms = std::max(1, uniformInt(program, "breakupCellforms", 1));
			const float belongsToTileX = std::floor(u * tilesX) / tilesX;
			const float belongsToTileY = std::floor(v * tilesY) / tilesY;
			const float gridRadius = sampleSlot(textures, 2, belongsToTileX, belongsToTileY).r;
			const SDL3GPUColorF source = sampleSlot(textures, 0, u, v);
			if (gridRadius >= 1.0f)
				return source;

			const int thisRadius = static_cast<int>(std::floor(gridRadius * breakupCellforms));
			const float xPercent = std::fmod(u, 1.0f / tilesX) * tilesX;
			const float yPercent = std::fmod(v, 1.0f / tilesY) * tilesY;
			const float interval = 1.0f / static_cast<float>(breakupCellforms);
			const float maskU = (thisRadius - 1) * interval + interval * xPercent;
			const float maskV = yPercent;
			return mulColor(source, sampleSlot(textures, 1, maskU, maskV).r);
		}

		case SDL3GPUShaderKind::ColorModification:
			return evaluateColorModification(program, textures, u, v);

		case SDL3GPUShaderKind::ColourConversion:
			return evaluateColourConversion(program, textures, u, v);

		case SDL3GPUShaderKind::CropByMask: {
			const SDL3GPUColorF img = sampleSlot(textures, 0, u, v);
			const float d = 1.0f / 512.0f;
			const float n = 1.0f / 9.0f;
			float mask = 0.0f;
			for (int dy = -1; dy <= 1; ++dy) {
				for (int dx = -1; dx <= 1; ++dx)
					mask += sampleSlot(textures, 1, u + dx * d, v + dy * d).r * n;
			}
			return mulColor(img, mask);
		}

		case SDL3GPUShaderKind::EffectTrvswave: {
			const int w = std::max(1, uniformInt(program, "script_width", textures[0].image ? textures[0].image->w : 1));
			const int h = std::max(1, uniformInt(program, "script_height", textures[0].image ? textures[0].image->h : 1));
			const int effectCounter = uniformInt(program, "effect_counter");
			const int duration = std::max(1, uniformInt(program, "duration", 1));
			constexpr float pi = 3.14159265358979323846f;
			constexpr float amplitudeMax = 18.0f;
			constexpr float waveEnd = 64.0f;
			constexpr float waveStart = 512.0f;
			float amplitude = 0.0f;
			float wavelength = waveStart;
			if (effectCounter * 2 < duration) {
				amplitude = amplitudeMax * static_cast<float>(2 * effectCounter) / duration;
				wavelength = 1.0f / (((1.0f / waveEnd - 1.0f / waveStart) * static_cast<float>(2 * effectCounter) / duration) + (1.0f / waveStart));
			} else {
				amplitude = amplitudeMax * static_cast<float>(2 * (duration - effectCounter)) / duration;
				wavelength = 1.0f / (((1.0f / waveEnd - 1.0f / waveStart) * static_cast<float>(2 * (duration - effectCounter)) / duration) + (1.0f / waveStart));
			}
			const int i = static_cast<int>(u * w);
			const int j = static_cast<int>(v * h);
			int ii = i + static_cast<int>(amplitude * std::sin(pi * 2.0f * static_cast<float>(j) / wavelength));
			ii = std::max(0, std::min(w - 1, ii));
			return sampleSlot(textures, 0, static_cast<float>(ii) / w, static_cast<float>(j) / h);
		}

		case SDL3GPUShaderKind::EffectWarp: {
			constexpr float pi = 3.14159265358f;
			float uvx = u * uniformFloat(program, "cx", 1.0f);
			float uvy = v * uniformFloat(program, "cy", 1.0f);
			const float x = uvx * 2.0f - 1.0f;
			const float y = uvy * 2.0f - 1.0f;
			const float radius = std::sqrt(x * x + y * y);
			float phi = std::atan2(y, x);
			const float cyclePerSec = uniformFloat(program, "animationClock") * pi * 2.0f;
			const float amplitude = uniformFloat(program, "amplitude");
			const float wavelength = std::max(uniformFloat(program, "wavelength", 1.0f), 0.0001f);
			const float speed = uniformFloat(program, "speed");
			phi += (amplitude * pi / 1000.0f) * std::sin((cyclePerSec * speed * 0.06f) + (1000.0f * 1920.0f * pi * radius) / (1080.0f * wavelength));
			uvx = (radius * std::cos(phi) + 1.0f) / 2.0f;
			uvy = (radius * std::sin(phi) + 1.0f) / 2.0f;
			const float cx = std::max(uniformFloat(program, "cx", 1.0f), 0.0001f);
			const float cy = std::max(uniformFloat(program, "cy", 1.0f), 0.0001f);
			return sampleSlot(textures, 0, clampFloat(uvx) / cx, clampFloat(uvy) / cy);
		}

		case SDL3GPUShaderKind::EffectWhirl: {
			const int effectCounter = uniformInt(program, "effect_counter");
			const int duration = std::max(1, uniformInt(program, "duration", 1));
			const int direction = uniformInt(program, "direction");
			const float renderW = std::max(uniformFloat(program, "render_width", textures[0].image ? textures[0].image->w : 1), 1.0f);
			const float renderH = std::max(uniformFloat(program, "render_height", textures[0].image ? textures[0].image->h : 1), 1.0f);
			const float textureW = std::max(uniformFloat(program, "texture_width", textures[0].image ? textures[0].image->w : 1), 1.0f);
			const float textureH = std::max(uniformFloat(program, "texture_height", textures[0].image ? textures[0].image->h : 1), 1.0f);
			constexpr float pi = 3.14159265358979323846f;
			constexpr float omega = pi / 64.0f;
			const float t = static_cast<float>(effectCounter) * pi / static_cast<float>(duration * 2);
			float radAmp = std::sin(2.0f * t);
			float radBase = 0.0f;
			int d = -1;
			if (direction == -1 || direction == 1) {
				radBase = 4.0f * t;
			} else if (direction == -2 || direction == 2) {
				const float oneMinusCos = 1.0f - std::cos(t);
				radAmp = pi * (std::sin(t) - oneMinusCos);
				radBase = pi * 2.0f * oneMinusCos + radAmp;
			}
			const float centerX = renderW / 2.0f;
			const float centerY = renderH / 2.0f;
			const float x = u * textureW - centerX;
			const float y = v * textureH - centerY;
			const float theta = static_cast<float>(d) * (radBase + radAmp * std::sin(std::sqrt(x * x + y * y) * omega));
			const float i = clampFloat(x * std::cos(theta) - y * std::sin(theta) + centerX, 0.0f, renderW - 1.0f);
			const float j = clampFloat(x * std::sin(theta) + y * std::cos(theta) + centerY, 0.0f, renderH - 1.0f);
			return sampleSlot(textures, 0, i / textureW, j / textureH);
		}

		case SDL3GPUShaderKind::GlassSmash:
			return insideUnitBox(u, v) ? mulColor(sampleSlot(textures, 0, u, v), uniformFloat(program, "alpha", 1.0f)) : SDL3GPUColorF{};

		case SDL3GPUShaderKind::GlyphGradient: {
			SDL3GPUColorF source = sampleSlot(textures, 0, u, v);
			const SDL3GPUColorF color = uniformVec4(program, "color");
			const int height = std::max(1, uniformInt(program, "height", textures[0].image ? textures[0].image->h : 1));
			const int maxy = uniformInt(program, "maxy");
			const int faceAscender = std::max(1, uniformInt(program, "faceAscender", 1));
			const float currentY = static_cast<float>(height) * (1.0f - v);
			const float currentAboveBaseline = currentY - static_cast<float>(height - maxy);
			const float percentAboveBaseline = std::max(0.0f, currentAboveBaseline / static_cast<float>(faceAscender));
			const bool isWhite = std::fabs(color.r - 1.0f) <= 0.5f / 255.0f &&
			                     std::fabs(color.g - 1.0f) <= 0.5f / 255.0f &&
			                     std::fabs(color.b - 1.0f) <= 0.5f / 255.0f;
			const float lightening = 0.5f * (isWhite ? (percentAboveBaseline - 0.6f) : (0.65f - percentAboveBaseline));
			source.r = (color.r + lightening) * source.a;
			source.g = (color.g + lightening) * source.a;
			source.b = (color.b + lightening) * source.a;
			return source;
		}

		case SDL3GPUShaderKind::MergeAlpha: {
			SDL3GPUColorF color = sampleSlot(textures, 0, u, v);
			const float alpha = sampleSlot(textures, 1, u, v).r;
			color.a = alpha;
			color.r *= alpha;
			color.g *= alpha;
			color.b *= alpha;
			return color;
		}

		case SDL3GPUShaderKind::MultiplyAlpha: {
			SDL3GPUColorF color = sampleSlot(textures, 0, u, v);
			color.r *= color.a;
			color.g *= color.a;
			color.b *= color.a;
			return color;
		}

		case SDL3GPUShaderKind::Pixelate: {
			const float width = std::max(static_cast<float>(uniformInt(program, "width", textures[0].image ? textures[0].image->w : 1)), 1.0f);
			const float height = std::max(static_cast<float>(uniformInt(program, "height", textures[0].image ? textures[0].image->h : 1)), 1.0f);
			const float factor = static_cast<float>(uniformInt(program, "factor"));
			const float cellW = (factor + 1.0f) / width;
			const float cellH = (factor + 1.0f) / height;
			const float sampleU = cellW * std::floor(u / cellW + 0.5f);
			const float sampleV = cellH * std::floor(v / cellH + 0.5f);
			SDL3GPUColorF color = sampleSlot(textures, 0, sampleU, sampleV);
			color.a = 1.0f;
			return color;
		}

		case SDL3GPUShaderKind::RenderSubtitles: {
			SDL3GPUColorF result{};
			const int ntextures = std::min(std::max(uniformInt(program, "ntextures"), 0), 8);
			const SDL3GPUColorF dstDims = uniformVec4(program, "dstDims", SDL3GPUColorF{
			                                             textures[0].image ? static_cast<float>(textures[0].image->w) : 1.0f,
			                                             textures[0].image ? static_cast<float>(textures[0].image->h) : 1.0f,
			                                             0.0f, 0.0f});
			const float absX = dstDims.r * u;
			const float absY = dstDims.g * v;
			for (int i = 0; i < ntextures; ++i) {
				const SDL3GPUColorF dims = uniformVec4(program, indexedUniformName("subDims", i).c_str());
				const SDL3GPUColorF coords = uniformVec4(program, indexedUniformName("subCoords", i).c_str());
				if (absX >= coords.r && absY >= coords.g && absX <= coords.r + dims.r && absY <= coords.g + dims.g) {
					const float sampleU = (absX - coords.r) / 2048.0f;
					const float sampleV = (absY - coords.g + static_cast<float>(i) * 256.0f) / (256.0f * 8.0f);
					const SDL3GPUColorF subColor = uniformVec4(program, indexedUniformName("subColors", i).c_str());
					const float alpha = sampleSlot(textures, 1, sampleU, sampleV).r * subColor.a;
					const SDL3GPUColorF col{alpha * subColor.r, alpha * subColor.g, alpha * subColor.b, alpha};
					result = addColor(col, mulColor(result, 1.0f - col.a));
				}
			}
			return result;
		}

		case SDL3GPUShaderKind::TextFade: {
			SDL3GPUColorF color = sampleSlot(textures, 0, u, v);
			const int current = static_cast<int>(u * static_cast<float>(uniformInt(program, "width", textures[0].image ? textures[0].image->w : 1)));
			const int full = uniformInt(program, "full");
			const int partial = uniformInt(program, "partial");
			if (current > full) {
				if (current >= full + partial)
					return SDL3GPUColorF{};
				if (partial > 0)
					color = mulColor(color, 1.0f - (static_cast<float>(current - full) / partial));
			}
			return color;
		}

		case SDL3GPUShaderKind::DefaultVertex:
		case SDL3GPUShaderKind::Unknown:
		default:
			return sampleSlot(textures, 0, u, v);
	}
}

void cpuBlit(GPU_Image *image, GPU_Rect *src_rect, GPU_Target *target, float x, float y,
             float degrees, float scaleX, float scaleY) {
	if (!image || !ensureTargetBacking(target) || scaleX == 0.0f || scaleY == 0.0f)
		return;
	ensureImagePixelsCurrent(image);
	ensureImagePixelsCurrent(target->image);

	const float srcX = src_rect ? src_rect->x : 0.0f;
	const float srcY = src_rect ? src_rect->y : 0.0f;
	const float srcW = src_rect ? src_rect->w : static_cast<float>(image->w);
	const float srcH = src_rect ? src_rect->h : static_cast<float>(image->h);
	if (srcW <= 0.0f || srcH <= 0.0f)
		return;

	const float destW = std::abs(srcW * scaleX);
	const float destH = std::abs(srcH * scaleY);
	if (destW < 1.0f || destH < 1.0f)
		return;

	const int x0 = static_cast<int>(std::floor(x - destW * 0.5f));
	const int y0 = static_cast<int>(std::floor(y - destH * 0.5f));
	const int x1 = static_cast<int>(std::ceil(x + destW * 0.5f));
	const int y1 = static_cast<int>(std::ceil(y + destH * 0.5f));
	constexpr float pi = 3.14159265358979323846f;
	const float radians = -degrees * pi / 180.0f;
	const float cosTheta = std::cos(radians);
	const float sinTheta = std::sin(radians);
	const bool rotated = std::abs(degrees) > 0.0001f;

	for (int dstY = y0; dstY < y1; ++dstY) {
		for (int dstX = x0; dstX < x1; ++dstX) {
			if (!targetAllowsPixel(target, dstX, dstY))
				continue;

			float localX = dstX + 0.5f - x;
			float localY = dstY + 0.5f - y;
			if (rotated) {
				const float unrotatedX = cosTheta * localX - sinTheta * localY;
				const float unrotatedY = sinTheta * localX + cosTheta * localY;
				localX = unrotatedX;
				localY = unrotatedY;
			}

			const float sourceLocalX = localX / scaleX + srcW * 0.5f;
			const float sourceLocalY = localY / scaleY + srcH * 0.5f;
			if (sourceLocalX < 0.0f || sourceLocalY < 0.0f || sourceLocalX >= srcW || sourceLocalY >= srcH)
				continue;

			const int sampleX = static_cast<int>(srcX + sourceLocalX);
			const int sampleY = static_cast<int>(srcY + sourceLocalY);
			SDL_Color source = modulatePixel(readImagePixel(image, sampleX, sampleY), image);
			if (source.a == 0 && image->use_blending)
				continue;

			const SDL_Color destination = readImagePixel(target->image, dstX, dstY);
			writeImagePixel(target->image, dstX, dstY, blendPixel(image, source, destination));
		}
	}

	uploadImage(target->image);
}

bool cpuShaderBlit(const SDL3GPUProgramObject &program, GPU_Image *image, GPU_Rect *src_rect, GPU_Target *target,
                   float x, float y, float degrees, float scaleX, float scaleY) {
	if (program.kind == SDL3GPUShaderKind::Unknown || program.kind == SDL3GPUShaderKind::DefaultVertex)
		return false;
	if (!image || !ensureTargetBacking(target) || scaleX == 0.0f || scaleY == 0.0f)
		return false;

	std::array<SDL3GPUTextureView, 8> textures{};
	std::array<std::vector<Uint8>, 8> snapshots{};
	textures[0].image = image;
	for (size_t i = 1; i < textures.size(); ++i)
		textures[i].image = program.images[i];

	for (size_t i = 0; i < textures.size(); ++i) {
		if (!textures[i].image)
			continue;
		ensureImagePixelsCurrent(textures[i].image);
		if (textures[i].image == target->image) {
			snapshots[i] = textures[i].image->pixels;
			textures[i].pixels = snapshots[i].data();
		} else {
			textures[i].pixels = textures[i].image->pixels.data();
		}
	}
	ensureImagePixelsCurrent(target->image);

	const float srcX = src_rect ? src_rect->x : 0.0f;
	const float srcY = src_rect ? src_rect->y : 0.0f;
	const float srcW = src_rect ? src_rect->w : static_cast<float>(image->w);
	const float srcH = src_rect ? src_rect->h : static_cast<float>(image->h);
	if (srcW <= 0.0f || srcH <= 0.0f)
		return false;

	const float destW = std::abs(srcW * scaleX);
	const float destH = std::abs(srcH * scaleY);
	if (destW < 1.0f || destH < 1.0f)
		return false;

	const int x0 = static_cast<int>(std::floor(x - destW * 0.5f));
	const int y0 = static_cast<int>(std::floor(y - destH * 0.5f));
	const int x1 = static_cast<int>(std::ceil(x + destW * 0.5f));
	const int y1 = static_cast<int>(std::ceil(y + destH * 0.5f));
	constexpr float pi = 3.14159265358979323846f;
	const float radians = -degrees * pi / 180.0f;
	const float cosTheta = std::cos(radians);
	const float sinTheta = std::sin(radians);
	const bool rotated = std::abs(degrees) > 0.0001f;

	for (int dstY = y0; dstY < y1; ++dstY) {
		for (int dstX = x0; dstX < x1; ++dstX) {
			if (!targetAllowsPixel(target, dstX, dstY))
				continue;

			float localX = dstX + 0.5f - x;
			float localY = dstY + 0.5f - y;
			if (rotated) {
				const float unrotatedX = cosTheta * localX - sinTheta * localY;
				const float unrotatedY = sinTheta * localX + cosTheta * localY;
				localX = unrotatedX;
				localY = unrotatedY;
			}

			const float sourceLocalX = localX / scaleX + srcW * 0.5f;
			const float sourceLocalY = localY / scaleY + srcH * 0.5f;
			if (sourceLocalX < 0.0f || sourceLocalY < 0.0f || sourceLocalX >= srcW || sourceLocalY >= srcH)
				continue;

			const float u = (srcX + sourceLocalX) / static_cast<float>(image->w);
			const float v = (srcY + sourceLocalY) / static_cast<float>(image->h);
			const SDL_Color source = toSDLColor(evaluateShaderPixel(program, textures, u, v));
			if (source.a == 0 && image->use_blending)
				continue;

			const SDL_Color destination = readImagePixel(target->image, dstX, dstY);
			writeImagePixel(target->image, dstX, dstY, blendPixel(image, source, destination));
		}
	}

	uploadImage(target->image);
	return true;
}

float edgeFunction(const SDL3GPUVertex &a, const SDL3GPUVertex &b, float x, float y) {
	return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x);
}

bool cpuShaderTriangles(const SDL3GPUProgramObject &program, GPU_Image *image, GPU_Target *target,
                        const SDL3GPUVertex *vertices, Uint32 numVertices,
                        const Uint16 *indices, Uint32 numIndices) {
	if (program.kind == SDL3GPUShaderKind::Unknown || program.kind == SDL3GPUShaderKind::DefaultVertex)
		return false;
	if (!image || !target || !vertices || !indices || numVertices == 0 || numIndices < 3 || !ensureTargetBacking(target))
		return false;

	std::array<SDL3GPUTextureView, 8> textures{};
	std::array<std::vector<Uint8>, 8> snapshots{};
	textures[0].image = image;
	for (size_t i = 1; i < textures.size(); ++i)
		textures[i].image = program.images[i];

	for (size_t i = 0; i < textures.size(); ++i) {
		if (!textures[i].image)
			continue;
		ensureImagePixelsCurrent(textures[i].image);
		if (textures[i].image == target->image) {
			snapshots[i] = textures[i].image->pixels;
			textures[i].pixels = snapshots[i].data();
		} else {
			textures[i].pixels = textures[i].image->pixels.data();
		}
	}
	ensureImagePixelsCurrent(target->image);

	for (Uint32 i = 0; i + 2 < numIndices; i += 3) {
		if (indices[i] >= numVertices || indices[i + 1] >= numVertices || indices[i + 2] >= numVertices)
			continue;

		const SDL3GPUVertex &a = vertices[indices[i]];
		const SDL3GPUVertex &b = vertices[indices[i + 1]];
		const SDL3GPUVertex &c = vertices[indices[i + 2]];
		const float area = edgeFunction(a, b, c.x, c.y);
		if (std::fabs(area) <= 0.00001f)
			continue;

		const int x0 = std::max(0, static_cast<int>(std::floor(std::min({a.x, b.x, c.x}))));
		const int y0 = std::max(0, static_cast<int>(std::floor(std::min({a.y, b.y, c.y}))));
		const int x1 = std::min<int>(target->image->w, static_cast<int>(std::ceil(std::max({a.x, b.x, c.x}))));
		const int y1 = std::min<int>(target->image->h, static_cast<int>(std::ceil(std::max({a.y, b.y, c.y}))));

		for (int y = y0; y < y1; ++y) {
			for (int x = x0; x < x1; ++x) {
				if (!targetAllowsPixel(target, x, y))
					continue;

				const float px = x + 0.5f;
				const float py = y + 0.5f;
				const float w0 = edgeFunction(b, c, px, py) / area;
				const float w1 = edgeFunction(c, a, px, py) / area;
				const float w2 = edgeFunction(a, b, px, py) / area;
				constexpr float epsilon = -0.0001f;
				if (w0 < epsilon || w1 < epsilon || w2 < epsilon)
					continue;

				const float u = w0 * a.s + w1 * b.s + w2 * c.s;
				const float v = w0 * a.t + w1 * b.t + w2 * c.t;
				SDL3GPUColorF fragment = evaluateShaderPixel(program, textures, u, v);
				fragment.r *= w0 * a.r + w1 * b.r + w2 * c.r;
				fragment.g *= w0 * a.g + w1 * b.g + w2 * c.g;
				fragment.b *= w0 * a.b + w1 * b.b + w2 * c.b;
				fragment.a *= w0 * a.a + w1 * b.a + w2 * c.a;

				const SDL_Color source = toSDLColor(fragment);
				if (source.a == 0 && image->use_blending)
					continue;
				const SDL_Color destination = readImagePixel(target->image, x, y);
				writeImagePixel(target->image, x, y, blendPixel(image, source, destination));
			}
		}
	}

	uploadImage(target->image);
	return true;
}

struct UploadedBuffer {
	SDL_GPUBuffer *buffer{nullptr};
	SDL_GPUTransferBuffer *transfer{nullptr};
	Uint32 size{0};
};

void releaseUploadedBuffer(UploadedBuffer &uploaded) {
	if (uploaded.buffer && rendererState.device)
		SDL_ReleaseGPUBuffer(rendererState.device, uploaded.buffer);
	if (uploaded.transfer && rendererState.device)
		SDL_ReleaseGPUTransferBuffer(rendererState.device, uploaded.transfer);
	uploaded = UploadedBuffer{};
}

bool prepareUploadedBuffer(UploadedBuffer &uploaded, SDL_GPUBufferUsageFlags usage, const void *data, Uint32 size) {
	if (!rendererState.device || !data || size == 0)
		return false;

	SDL_GPUBufferCreateInfo bufferInfo{};
	bufferInfo.usage = usage;
	bufferInfo.size  = size;
	uploaded.buffer = SDL_CreateGPUBuffer(rendererState.device, &bufferInfo);
	if (!uploaded.buffer)
		return false;

	SDL_GPUTransferBufferCreateInfo transferInfo{};
	transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	transferInfo.size  = size;
	uploaded.transfer = SDL_CreateGPUTransferBuffer(rendererState.device, &transferInfo);
	if (!uploaded.transfer) {
		releaseUploadedBuffer(uploaded);
		return false;
	}

	void *mapped = SDL_MapGPUTransferBuffer(rendererState.device, uploaded.transfer, false);
	if (!mapped) {
		releaseUploadedBuffer(uploaded);
		return false;
	}
	std::memcpy(mapped, data, size);
	SDL_UnmapGPUTransferBuffer(rendererState.device, uploaded.transfer);
	uploaded.size = size;
	return true;
}

void encodeBufferUpload(SDL_GPUCopyPass *copyPass, const UploadedBuffer &uploaded) {
	SDL_GPUTransferBufferLocation source{};
	source.transfer_buffer = uploaded.transfer;
	SDL_GPUBufferRegion destination{};
	destination.buffer = uploaded.buffer;
	destination.size   = uploaded.size;
	SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);
}

SDL_Rect targetScissor(const GPU_Target *target) {
	SDL_Rect scissor{0, 0, target ? target->w : 0, target ? target->h : 0};
	if (!target || !target->use_clip_rect)
		return scissor;

	const int x0 = std::max<int>(0, static_cast<int>(std::floor(target->clip_rect.x)));
	const int y0 = std::max<int>(0, static_cast<int>(std::floor(target->clip_rect.y)));
	const int x1 = std::min<int>(target->w, static_cast<int>(std::ceil(target->clip_rect.x + target->clip_rect.w)));
	const int y1 = std::min<int>(target->h, static_cast<int>(std::ceil(target->clip_rect.y + target->clip_rect.h)));
	scissor.x = x0;
	scissor.y = y0;
	scissor.w = std::max(0, x1 - x0);
	scissor.h = std::max(0, y1 - y0);
	return scissor;
}

bool renderNativeProgramIndexedTriangles(const SDL3GPUProgramObject &program,
                                         GPU_Image *image, GPU_Target *target,
                                         const SDL3GPUVertex *vertices, Uint32 numVertices,
                                         const Uint16 *indices, Uint32 numIndices) {
	if (!program.nativeFragmentShader) {
		setShaderMessage("SDL3 native shader program has no fragment shader");
		return false;
	}
	if (!rendererState.device || !image || !target || !vertices || !indices || numVertices == 0 || numIndices == 0)
		return false;
	if (!image->texture || !ensureTargetBacking(target) || !target->texture || !target->image)
		return false;
	if (target->image == image)
		return false;
	if (!isNativeTextureFormat(image->format) || !isNativeTextureFormat(target->image->format))
		return false;
	if (!ensureNativeShaders())
		return false;

	SDL_GPUShader *vertexShader = program.nativeVertexShader ? program.nativeVertexShader : texturedVertexShader;
	SDL_GPUGraphicsPipeline *pipeline = getPipeline(textureFormat(target->image->format),
	                                                image->use_blending,
	                                                image->blend_mode,
	                                                vertexShader,
	                                                program.nativeFragmentShader);
	if (!pipeline)
		return false;

	UploadedBuffer vertexUpload;
	UploadedBuffer indexUpload;
	const Uint32 vertexBytes = static_cast<Uint32>(numVertices * sizeof(SDL3GPUVertex));
	const Uint32 indexBytes  = static_cast<Uint32>(numIndices * sizeof(Uint16));
	if (!prepareUploadedBuffer(vertexUpload, SDL_GPU_BUFFERUSAGE_VERTEX, vertices, vertexBytes) ||
	    !prepareUploadedBuffer(indexUpload, SDL_GPU_BUFFERUSAGE_INDEX, indices, indexBytes)) {
		releaseUploadedBuffer(vertexUpload);
		releaseUploadedBuffer(indexUpload);
		return false;
	}

	SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(rendererState.device);
	if (!commands) {
		releaseUploadedBuffer(vertexUpload);
		releaseUploadedBuffer(indexUpload);
		return false;
	}

	SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(commands);
	encodeBufferUpload(copyPass, vertexUpload);
	encodeBufferUpload(copyPass, indexUpload);
	SDL_EndGPUCopyPass(copyPass);

	SDL_GPUColorTargetInfo colorTarget{};
	colorTarget.texture  = target->texture;
	colorTarget.load_op  = SDL_GPU_LOADOP_LOAD;
	colorTarget.store_op = SDL_GPU_STOREOP_STORE;

	SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(commands, &colorTarget, 1, nullptr);
	if (!renderPass) {
		SDL_CancelGPUCommandBuffer(commands);
		releaseUploadedBuffer(vertexUpload);
		releaseUploadedBuffer(indexUpload);
		return false;
	}

	SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

	SDL_GPUBufferBinding vertexBinding{};
	vertexBinding.buffer = vertexUpload.buffer;
	SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

	SDL_GPUBufferBinding indexBinding{};
	indexBinding.buffer = indexUpload.buffer;
	SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

	const Uint32 samplerCount = std::min<Uint32>(program.nativeFragmentResources.numSamplers,
	                                             static_cast<Uint32>(program.images.size()));
	std::vector<SDL_GPUTextureSamplerBinding> samplerBindings;
	samplerBindings.reserve(samplerCount);
	for (Uint32 i = 0; i < samplerCount; ++i) {
		GPU_Image *boundImage = program.images[i];
		if (i == 0 && !boundImage)
			boundImage = image;
		if (!boundImage || !boundImage->texture) {
			SDL_EndGPURenderPass(renderPass);
			SDL_CancelGPUCommandBuffer(commands);
			releaseUploadedBuffer(vertexUpload);
			releaseUploadedBuffer(indexUpload);
			setShaderMessage("SDL3 native shader program is missing a bound sampler texture");
			return false;
		}
		SDL_GPUSampler *sampler = getSampler(boundImage->filter_mode);
		if (!sampler) {
			SDL_EndGPURenderPass(renderPass);
			SDL_CancelGPUCommandBuffer(commands);
			releaseUploadedBuffer(vertexUpload);
			releaseUploadedBuffer(indexUpload);
			return false;
		}
		SDL_GPUTextureSamplerBinding binding{};
		binding.texture = boundImage->texture;
		binding.sampler = sampler;
		samplerBindings.push_back(binding);
	}
	if (!samplerBindings.empty())
		SDL_BindGPUFragmentSamplers(renderPass, 0, samplerBindings.data(), static_cast<Uint32>(samplerBindings.size()));

	struct VertexUniforms {
		float mvp[4][4];
	} vertexUniforms{};
	const float viewportW = target->viewport.w > 0.0f ? target->viewport.w : static_cast<float>(target->w);
	const float viewportH = target->viewport.h > 0.0f ? target->viewport.h : static_cast<float>(target->h);
	vertexUniforms.mvp[0][0] = 2.0f / viewportW;
	vertexUniforms.mvp[1][1] = -2.0f / viewportH;
	vertexUniforms.mvp[2][2] = 1.0f;
	vertexUniforms.mvp[3][0] = -1.0f;
	vertexUniforms.mvp[3][1] = 1.0f;
	vertexUniforms.mvp[3][3] = 1.0f;
	SDL_PushGPUVertexUniformData(commands, 0, &vertexUniforms, sizeof(vertexUniforms));

	if (!program.nativeUniformRegisters.empty()) {
		SDL_PushGPUFragmentUniformData(commands, 0,
		                               program.nativeUniformRegisters.data(),
		                               static_cast<Uint32>(program.nativeUniformRegisters.size() * sizeof(SDL3GPUNativeUniformRegister)));
	}

	SDL_GPUViewport viewport{};
	viewport.x = target->viewport.x;
	viewport.y = target->viewport.y;
	viewport.w = viewportW;
	viewport.h = viewportH;
	viewport.min_depth = 0.0f;
	viewport.max_depth = 1.0f;
	SDL_SetGPUViewport(renderPass, &viewport);

	const SDL_Rect scissor = targetScissor(target);
	SDL_SetGPUScissor(renderPass, &scissor);

	SDL_DrawGPUIndexedPrimitives(renderPass, numIndices, 1, 0, 0, 0);
	SDL_EndGPURenderPass(renderPass);
	const bool submitted = SDL_SubmitGPUCommandBuffer(commands);
	releaseUploadedBuffer(vertexUpload);
	releaseUploadedBuffer(indexUpload);

	if (submitted)
		target->image->pixels_dirty = true;
	return submitted;
}

bool renderNativeIndexedTriangles(GPU_Image *image, GPU_Target *target, const SDL3GPUVertex *vertices,
                                  Uint32 numVertices, const Uint16 *indices, Uint32 numIndices) {
	if (!rendererState.device || !image || !target || !vertices || !indices || numVertices == 0 || numIndices == 0)
		return false;

	if (auto *program = activeProgramObject()) {
		if (program->nativeFragmentShader) {
			if (!renderNativeProgramIndexedTriangles(*program, image, target, vertices, numVertices, indices, numIndices))
				setShaderMessage("SDL3_GPU backend could not execute the active native shader program");
			return true;
		}
		if (cpuShaderTriangles(*program, image, target, vertices, numVertices, indices, numIndices))
			return true;
		setShaderMessage("SDL3_GPU backend cannot execute the active external triangle shader program");
		return true;
	}

	if (!image->texture || !ensureTargetBacking(target) || !target->texture || !target->image)
		return false;
	if (target->image == image)
		return false;
	if (!isNativeTextureFormat(image->format) || !isNativeTextureFormat(target->image->format))
		return false;

	SDL_GPUGraphicsPipeline *pipeline = getPipeline(textureFormat(target->image->format), image->use_blending, image->blend_mode);
	SDL_GPUSampler *sampler = getSampler(image->filter_mode);
	if (!pipeline || !sampler)
		return false;

	UploadedBuffer vertexUpload;
	UploadedBuffer indexUpload;
	const Uint32 vertexBytes = static_cast<Uint32>(numVertices * sizeof(SDL3GPUVertex));
	const Uint32 indexBytes  = static_cast<Uint32>(numIndices * sizeof(Uint16));
	if (!prepareUploadedBuffer(vertexUpload, SDL_GPU_BUFFERUSAGE_VERTEX, vertices, vertexBytes) ||
	    !prepareUploadedBuffer(indexUpload, SDL_GPU_BUFFERUSAGE_INDEX, indices, indexBytes)) {
		releaseUploadedBuffer(vertexUpload);
		releaseUploadedBuffer(indexUpload);
		return false;
	}

	SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(rendererState.device);
	if (!commands) {
		releaseUploadedBuffer(vertexUpload);
		releaseUploadedBuffer(indexUpload);
		return false;
	}

	SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(commands);
	encodeBufferUpload(copyPass, vertexUpload);
	encodeBufferUpload(copyPass, indexUpload);
	SDL_EndGPUCopyPass(copyPass);

	SDL_GPUColorTargetInfo colorTarget{};
	colorTarget.texture  = target->texture;
	colorTarget.load_op  = SDL_GPU_LOADOP_LOAD;
	colorTarget.store_op = SDL_GPU_STOREOP_STORE;

	SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(commands, &colorTarget, 1, nullptr);
	if (!renderPass) {
		SDL_CancelGPUCommandBuffer(commands);
		releaseUploadedBuffer(vertexUpload);
		releaseUploadedBuffer(indexUpload);
		return false;
	}

	SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

	SDL_GPUBufferBinding vertexBinding{};
	vertexBinding.buffer = vertexUpload.buffer;
	SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

	SDL_GPUBufferBinding indexBinding{};
	indexBinding.buffer = indexUpload.buffer;
	SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

	SDL_GPUTextureSamplerBinding samplerBinding{};
	samplerBinding.texture = image->texture;
	samplerBinding.sampler = sampler;
	SDL_BindGPUFragmentSamplers(renderPass, 0, &samplerBinding, 1);

	struct VertexUniforms {
		float mvp[4][4];
	} vertexUniforms{};
	const float viewportW = target->viewport.w > 0.0f ? target->viewport.w : static_cast<float>(target->w);
	const float viewportH = target->viewport.h > 0.0f ? target->viewport.h : static_cast<float>(target->h);
	vertexUniforms.mvp[0][0] = 2.0f / viewportW;
	vertexUniforms.mvp[1][1] = -2.0f / viewportH;
	vertexUniforms.mvp[2][2] = 1.0f;
	vertexUniforms.mvp[3][0] = -1.0f;
	vertexUniforms.mvp[3][1] = 1.0f;
	vertexUniforms.mvp[3][3] = 1.0f;
	SDL_PushGPUVertexUniformData(commands, 0, &vertexUniforms, sizeof(vertexUniforms));

	const float colorScale = 1.0f;
	SDL_PushGPUFragmentUniformData(commands, 0, &colorScale, sizeof(colorScale));

	SDL_GPUViewport viewport{};
	viewport.x = target->viewport.x;
	viewport.y = target->viewport.y;
	viewport.w = viewportW;
	viewport.h = viewportH;
	viewport.min_depth = 0.0f;
	viewport.max_depth = 1.0f;
	SDL_SetGPUViewport(renderPass, &viewport);

	const SDL_Rect scissor = targetScissor(target);
	SDL_SetGPUScissor(renderPass, &scissor);

	SDL_DrawGPUIndexedPrimitives(renderPass, numIndices, 1, 0, 0, 0);
	SDL_EndGPURenderPass(renderPass);
	const bool submitted = SDL_SubmitGPUCommandBuffer(commands);
	releaseUploadedBuffer(vertexUpload);
	releaseUploadedBuffer(indexUpload);

	if (submitted)
		target->image->pixels_dirty = true;
	return submitted;
}

bool nativeBlit(GPU_Image *image, GPU_Rect *src_rect, GPU_Target *target, float x, float y,
                float degrees, float scaleX, float scaleY) {
	if (!image || scaleX == 0.0f || scaleY == 0.0f)
		return false;

	if (auto *program = activeProgramObject()) {
		if (!program->nativeFragmentShader) {
			if (cpuShaderBlit(*program, image, src_rect, target, x, y, degrees, scaleX, scaleY))
				return true;
			setShaderMessage("SDL3_GPU backend cannot execute the active external shader program");
			return true;
		}
	}

	const float srcX = src_rect ? src_rect->x : 0.0f;
	const float srcY = src_rect ? src_rect->y : 0.0f;
	const float srcW = src_rect ? src_rect->w : static_cast<float>(image->w);
	const float srcH = src_rect ? src_rect->h : static_cast<float>(image->h);
	if (srcW <= 0.0f || srcH <= 0.0f)
		return false;

	const SDL_Color color = image->color;
	const float r = color.r / 255.0f;
	const float g = color.g / 255.0f;
	const float b = color.b / 255.0f;
	const float a = color.a / 255.0f;
	const float u0 = srcX / image->w;
	const float v0 = srcY / image->h;
	const float u1 = (srcX + srcW) / image->w;
	const float v1 = (srcY + srcH) / image->h;

	constexpr float pi = 3.14159265358979323846f;
	const float radians = degrees * pi / 180.0f;
	const float cosTheta = std::cos(radians);
	const float sinTheta = std::sin(radians);
	const float halfW = srcW * scaleX * 0.5f;
	const float halfH = srcH * scaleY * 0.5f;

	auto transform = [&](float localX, float localY, float u, float v) {
		SDL3GPUVertex vertex{};
		vertex.x = x + localX * cosTheta - localY * sinTheta;
		vertex.y = y + localX * sinTheta + localY * cosTheta;
		vertex.r = r;
		vertex.g = g;
		vertex.b = b;
		vertex.a = a;
		vertex.s = u;
		vertex.t = v;
		return vertex;
	};

	std::array<SDL3GPUVertex, 4> vertices{
	    transform(-halfW, -halfH, u0, v0),
	    transform(halfW, -halfH, u1, v0),
	    transform(-halfW, halfH, u0, v1),
	    transform(halfW, halfH, u1, v1)};
	std::array<Uint16, 6> indices{{0, 1, 2, 2, 1, 3}};
	return renderNativeIndexedTriangles(image, target, vertices.data(), static_cast<Uint32>(vertices.size()),
	                                    indices.data(), static_cast<Uint32>(indices.size()));
}

SDL_GPUFilter toSDLFilter(GPU_FilterEnum filter) {
	return filter == GPU_FILTER_NEAREST ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
}

bool presentTarget(GPU_Target *target, SDL_GPUCommandBuffer *commands, SDL_GPUTexture *swapchainTexture, Uint32 width, Uint32 height) {
	if (!target || !target->image || !target->texture || !commands || !swapchainTexture)
		return false;

	SDL_GPUBlitInfo blit{};
	blit.source.texture      = target->texture;
	blit.source.w            = target->image->w;
	blit.source.h            = target->image->h;
	blit.destination.texture = swapchainTexture;
	blit.destination.w       = width;
	blit.destination.h       = height;
	blit.load_op             = SDL_GPU_LOADOP_DONT_CARE;
	blit.flip_mode           = SDL_FLIP_NONE;
	blit.filter              = toSDLFilter(target->image->filter_mode);
	blit.cycle               = false;
	SDL_BlitGPUTexture(commands, &blit);
	return true;
}
} // namespace

GPU_RendererID SDLCALL GPU_MakeRendererID(const char *name, GPU_RendererEnum renderer, int major_version, int minor_version) {
	GPU_RendererID id{};
	id.name          = name;
	id.renderer      = renderer;
	id.major_version = major_version;
	id.minor_version = minor_version;
	return id;
}

void SDLCALL GPU_SetPreInitFlags(GPU_InitFlagEnum GPU_flags) {
	pendingPreinitFlags = GPU_flags;
}

GPU_Target *SDLCALL GPU_InitRendererByID(GPU_RendererID renderer_request, Uint16 w, Uint16 h, GPU_WindowFlagEnum SDL_flags) {
	GPU_Quit();

	const SDL_GPUShaderFormat shaderFormats = SDL_GPU_SHADERFORMAT_SPIRV |
	                                          SDL_GPU_SHADERFORMAT_DXBC |
	                                          SDL_GPU_SHADERFORMAT_DXIL |
	                                          SDL_GPU_SHADERFORMAT_MSL |
	                                          SDL_GPU_SHADERFORMAT_METALLIB;

	rendererState.device = SDL_CreateGPUDevice(shaderFormats, rendererState.debug_level == GPU_DEBUG_LEVEL_MAX, nullptr);
	if (!rendererState.device)
		return nullptr;

	rendererState.window = SDL_CreateWindow("ONScripter-RU", w, h, SDL_flags);
	if (!rendererState.window) {
		GPU_Quit();
		return nullptr;
	}

	if (!SDL_ClaimWindowForGPUDevice(rendererState.device, rendererState.window)) {
		GPU_Quit();
		return nullptr;
	}

	const SDL_GPUPresentMode presentMode = (pendingPreinitFlags & GPU_INIT_DISABLE_VSYNC) ?
	                                           SDL_GPU_PRESENTMODE_IMMEDIATE :
	                                           SDL_GPU_PRESENTMODE_VSYNC;
	SDL_SetGPUSwapchainParameters(rendererState.device, rendererState.window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, presentMode);

	auto *target         = new GPU_Target{};
	auto *context        = new GPU_Context{};
	context->windowID    = SDL_GetWindowID(rendererState.window);
	context->window_w    = w;
	context->window_h    = h;
	context->drawable_w  = w;
	context->drawable_h  = h;
	context->refcount    = 1;
	context->shapes_use_blending = true;

	target->renderer       = &rendererState;
	target->context_target = target;
	target->w              = w;
	target->h              = h;
	target->base_w         = w;
	target->base_h         = h;
	target->viewport       = GPU_Rect{0, 0, static_cast<float>(w), static_cast<float>(h)};
	target->context        = context;
	target->refcount       = 1;
	target->is_window      = true;

	rendererState.id                     = renderer_request;
	rendererState.current_context_target = target;
	rendererState.preinit_flags          = pendingPreinitFlags;
	rendererState.swapchain_format       = SDL_GetGPUSwapchainTextureFormat(rendererState.device, rendererState.window);

	if (!resizeTargetBacking(target, w, h)) {
		GPU_Quit();
		return nullptr;
	}

	return target;
}

void SDLCALL GPU_Quit(void) {
	if (rendererState.current_context_target) {
		auto *target = rendererState.current_context_target;
		if (target->image && !target->image->target) {
			releaseImageTexture(target->image);
			delete target->image;
			target->image = nullptr;
			target->texture = nullptr;
		}
		delete target->context;
		delete target;
		rendererState.current_context_target = nullptr;
	}

	if (rendererState.device && rendererState.window) {
		SDL_ReleaseWindowFromGPUDevice(rendererState.device, rendererState.window);
	}

	releaseNativeRendererObjects();

	if (rendererState.window) {
		SDL_DestroyWindow(rendererState.window);
		rendererState.window = nullptr;
	}

	if (rendererState.device) {
		SDL_DestroyGPUDevice(rendererState.device);
		rendererState.device = nullptr;
	}
}

void SDLCALL GPU_SetDebugLevel(GPU_DebugLevelEnum level) {
	rendererState.debug_level = level;
}

GPU_Renderer *SDLCALL GPU_GetCurrentRenderer(void) {
	return rendererState.device ? &rendererState : nullptr;
}

GPU_Target *SDLCALL GPU_GetContextTarget(void) {
	return rendererState.current_context_target;
}

GPU_bool SDLCALL GPU_SetWindowResolution(Uint16 w, Uint16 h) {
	if (!rendererState.window || !rendererState.current_context_target)
		return false;

	if (!SDL_SetWindowSize(rendererState.window, w, h))
		return false;

	auto *target = rendererState.current_context_target;
	target->base_w = w;
	target->base_h = h;
	if (!target->using_virtual_resolution) {
		target->w = w;
		target->h = h;
		target->viewport = GPU_Rect{0, 0, static_cast<float>(w), static_cast<float>(h)};
		resizeTargetBacking(target, w, h);
	}
	if (target->context) {
		target->context->window_w   = w;
		target->context->window_h   = h;
		target->context->drawable_w = w;
		target->context->drawable_h = h;
	}
	return true;
}

void SDLCALL GPU_SetShapeBlending(GPU_bool enable) {
	if (rendererState.current_context_target && rendererState.current_context_target->context)
		rendererState.current_context_target->context->shapes_use_blending = enable;
}

GPU_Target *SDLCALL GPU_GetTarget(GPU_Image *image) {
	if (!image)
		return nullptr;

	if (image->target)
		return image->target;

	auto *target         = new GPU_Target{};
	target->renderer     = image->renderer;
	target->context_target = image->context_target;
	target->image        = image;
	target->w            = image->w;
	target->h            = image->h;
	target->base_w       = image->w;
	target->base_h       = image->h;
	target->viewport     = GPU_Rect{0, 0, static_cast<float>(image->w), static_cast<float>(image->h)};
	target->refcount     = 1;
	target->texture      = image->texture;

	image->target = target;
	return target;
}

void SDLCALL GPU_SetVirtualResolution(GPU_Target *target, Uint16 w, Uint16 h) {
	if (!target)
		return;
	target->w = w;
	target->h = h;
	target->viewport = GPU_Rect{0, 0, static_cast<float>(w), static_cast<float>(h)};
	target->using_virtual_resolution = true;
	if (target->is_window)
		resizeTargetBacking(target, w, h);
}

GPU_Rect SDLCALL GPU_SetClipRect(GPU_Target *target, GPU_Rect rect) {
	GPU_Rect previous{};
	if (!target)
		return previous;
	previous              = target->clip_rect;
	target->clip_rect      = rect;
	target->use_clip_rect  = true;
	return previous;
}

void SDLCALL GPU_UnsetClip(GPU_Target *target) {
	if (target)
		target->use_clip_rect = false;
}

GPU_Image *SDLCALL GPU_CreateImage(Uint16 w, Uint16 h, GPU_FormatEnum format) {
	auto *image = new GPU_Image{};
	initialiseImageDefaults(image, w, h, format);
	createTexture(image);
	return image;
}

GPU_Image *SDLCALL GPU_CopyImage(GPU_Image *image) {
	if (!image)
		return nullptr;
	ensureImagePixelsCurrent(image);
	GPU_Image *copy = GPU_CreateImage(image->w, image->h, image->format);
	copy->pixels    = image->pixels;
	copy->pitch     = image->pitch;
	copy->color     = image->color;
	copy->use_blending = image->use_blending;
	copy->blend_mode = image->blend_mode;
	copy->snap_mode  = image->snap_mode;
	copy->filter_mode = image->filter_mode;
	uploadImage(copy);
	return copy;
}

void SDLCALL GPU_FreeImage(GPU_Image *image) {
	if (!image)
		return;

	if (image->refcount > 1) {
		--image->refcount;
		return;
	}

	if (image->texture && rendererState.device)
		SDL_ReleaseGPUTexture(rendererState.device, image->texture);
	delete image->target;
	delete image;
}

void SDLCALL GPU_UpdateImage(GPU_Image *image, const GPU_Rect *image_rect, SDL_Surface *surface, const GPU_Rect *surface_rect) {
	if (!image || !surface)
		return;
	if (image_rect)
		ensureImagePixelsCurrent(image);

	SDL_Surface *working = surface;
	bool freeWorking     = false;
	if (image->bytes_per_pixel == 4 && onsSurfaceBytesPerPixel(surface) != 4) {
		working = onsConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA8888, SDL_SWSURFACE);
		freeWorking = working != nullptr;
	}

	if (!working)
		return;

	const int srcBpp = onsSurfaceBytesPerPixel(working);
	const int srcX   = surface_rect ? static_cast<int>(surface_rect->x) : 0;
	const int srcY   = surface_rect ? static_cast<int>(surface_rect->y) : 0;
	const int dstX   = image_rect ? static_cast<int>(image_rect->x) : 0;
	const int dstY   = image_rect ? static_cast<int>(image_rect->y) : 0;
	const int width  = image_rect ? static_cast<int>(image_rect->w) : (surface_rect ? static_cast<int>(surface_rect->w) : working->w);
	const int height = image_rect ? static_cast<int>(image_rect->h) : (surface_rect ? static_cast<int>(surface_rect->h) : working->h);

	if (SDL_MUSTLOCK(working) && !SDL_LockSurface(working)) {
		if (freeWorking)
			SDL_FreeSurface(working);
		return;
	}

	for (int y = 0; y < height; ++y) {
		if (dstY + y < 0 || dstY + y >= image->h || srcY + y < 0 || srcY + y >= working->h)
			continue;
		const auto *src = static_cast<const Uint8 *>(working->pixels) + (srcY + y) * working->pitch + srcX * srcBpp;
		auto *dst       = image->pixels.data() + (dstY + y) * image->pitch + dstX * image->bytes_per_pixel;
		copyPixelRow(dst, image->bytes_per_pixel, src, srcBpp, std::min<int>(width, image->w - dstX));
	}

	if (SDL_MUSTLOCK(working))
		SDL_UnlockSurface(working);
	if (freeWorking)
		SDL_FreeSurface(working);

	uploadImage(image);
}

void SDLCALL GPU_UpdateImageBytes(GPU_Image *image, const GPU_Rect *image_rect, const unsigned char *bytes, int bytes_per_row) {
	if (!image || !bytes)
		return;
	if (image_rect)
		ensureImagePixelsCurrent(image);

	const int dstX   = image_rect ? static_cast<int>(image_rect->x) : 0;
	const int dstY   = image_rect ? static_cast<int>(image_rect->y) : 0;
	const int width  = image_rect ? static_cast<int>(image_rect->w) : image->w;
	const int height = image_rect ? static_cast<int>(image_rect->h) : image->h;
	const int rowBytes = std::min(width * image->bytes_per_pixel, bytes_per_row);

	for (int y = 0; y < height; ++y) {
		if (dstY + y < 0 || dstY + y >= image->h)
			continue;
		auto *dst = image->pixels.data() + (dstY + y) * image->pitch + dstX * image->bytes_per_pixel;
		std::memcpy(dst, bytes + y * bytes_per_row, rowBytes);
	}

	uploadImage(image);
}

GPU_bool SDLCALL GPU_SaveImage(GPU_Image *image, const char *filename, GPU_FileFormatEnum format) {
	if (!image || !filename)
		return false;

	SDL_Surface *surface = GPU_CopySurfaceFromImage(image);
	if (!surface)
		return false;
	bool saved = false;
	if (format == GPU_FILE_PNG || (format == GPU_FILE_AUTO && extensionIsPng(filename))) {
		SDL_RWops *rwops = SDL_RWFromFile(filename, "wb");
		saved = saveSurfacePNG_RW(surface, rwops, true);
	} else if (format == GPU_FILE_BMP || (format == GPU_FILE_AUTO && extensionIsBmp(filename))) {
		saved = SDL_SaveBMP(surface, filename);
	} else {
		SDL_SetError("Unsupported SDL3_GPU image save format");
	}
	SDL_FreeSurface(surface);
	return saved;
}

GPU_bool SDLCALL GPU_SaveImage_RW(GPU_Image *image, SDL_RWops *rwops, GPU_bool free_rwops, GPU_FileFormatEnum format) {
	if (!image || !rwops)
		return false;

	SDL_Surface *surface = GPU_CopySurfaceFromImage(image);
	if (!surface) {
		if (free_rwops)
			SDL_RWclose(rwops);
		return false;
	}
	bool saved = false;
	if (format == GPU_FILE_PNG) {
		saved = saveSurfacePNG_RW(surface, rwops, free_rwops);
	} else if (format == GPU_FILE_BMP) {
		saved = SDL_SaveBMP_RW(surface, rwops, free_rwops);
	} else {
		SDL_SetError("Unsupported SDL3_GPU image save format");
		if (free_rwops)
			SDL_RWclose(rwops);
	}
	SDL_FreeSurface(surface);
	return saved;
}

void SDLCALL GPU_GenerateMipmaps(GPU_Image *image) {
	if (!image || !image->texture || !rendererState.device)
		return;
	SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(rendererState.device);
	if (!commands)
		return;
	SDL_GenerateMipmapsForGPUTexture(commands, image->texture);
	SDL_SubmitGPUCommandBuffer(commands);
	image->has_mipmaps = true;
}

void SDLCALL GPU_SetRGBA(GPU_Image *image, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
	if (image)
		image->color = SDL_Color{r, g, b, a};
}

void SDLCALL GPU_SetBlending(GPU_Image *image, GPU_bool enable) {
	if (image)
		image->use_blending = enable;
}

void SDLCALL GPU_SetBlendMode(GPU_Image *image, GPU_BlendPresetEnum mode) {
	if (!image)
		return;
	if (mode == GPU_BLEND_ADD) {
		image->blend_mode.source_color = GPU_FUNC_SRC_ALPHA;
		image->blend_mode.dest_color   = GPU_FUNC_ONE;
	} else {
		image->blend_mode = normalBlendMode();
	}
}

void SDLCALL GPU_SetImageFilter(GPU_Image *image, GPU_FilterEnum filter) {
	if (image)
		image->filter_mode = filter;
}

void SDLCALL GPU_SetSnapMode(GPU_Image *image, GPU_SnapEnum mode) {
	if (image)
		image->snap_mode = mode;
}

GPU_Image *SDLCALL GPU_CopyImageFromSurface(SDL_Surface *surface) {
	if (!surface)
		return nullptr;
	GPU_Image *image = GPU_CreateImage(surface->w, surface->h, onsSurfaceBytesPerPixel(surface) == 4 ? GPU_FORMAT_RGBA : GPU_FORMAT_RGB);
	GPU_UpdateImage(image, nullptr, surface, nullptr);
	return image;
}

GPU_Image *SDLCALL GPU_CopyImageFromTarget(GPU_Target *target) {
	if (!target)
		return nullptr;
	ensureTargetBacking(target);
	if (target->image)
		return GPU_CopyImage(target->image);

	GPU_Image *image = GPU_CreateImage(target->w, target->h, GPU_FORMAT_RGBA);
	return image;
}

SDL_Surface *SDLCALL GPU_CopySurfaceFromImage(GPU_Image *image) {
	if (!image)
		return nullptr;
	ensureImagePixelsCurrent(image);

	SDL_Surface *surface = onsCreateRGBSurface(SDL_SWSURFACE, image->w, image->h, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	if (!surface)
		return nullptr;

	if (SDL_MUSTLOCK(surface) && !SDL_LockSurface(surface)) {
		SDL_FreeSurface(surface);
		return nullptr;
	}

	for (int y = 0; y < image->h; ++y) {
		auto *dst       = static_cast<Uint8 *>(surface->pixels) + y * surface->pitch;
		const auto *src = image->pixels.data() + y * image->pitch;
		copyPixelRow(dst, onsSurfaceBytesPerPixel(surface), src, image->bytes_per_pixel, image->w);
	}

	if (SDL_MUSTLOCK(surface))
		SDL_UnlockSurface(surface);
	return surface;
}

void SDLCALL GPU_MatrixMode(int matrix_mode) {
	if (rendererState.current_context_target && rendererState.current_context_target->context)
		rendererState.current_context_target->context->matrix_mode = matrix_mode;
}

void SDLCALL GPU_PushMatrix(void) {}
void SDLCALL GPU_PopMatrix(void) {}
void SDLCALL GPU_LoadIdentity(void) {}
void SDLCALL GPU_Frustum(float, float, float, float, float, float) {}

void SDLCALL GPU_ClearRGBA(GPU_Target *target, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
	if (!target)
		return;
	target->color = SDL_Color{r, g, b, a};
	target->use_color = true;

	if (ensureTargetBacking(target)) {
		for (int y = 0; y < target->image->h; ++y) {
			auto *row = target->image->pixels.data() + y * target->image->pitch;
			for (int x = 0; x < target->image->w; ++x)
				copyPixelRow(row + x * target->image->bytes_per_pixel, target->image->bytes_per_pixel, reinterpret_cast<const Uint8 *>(&target->color), 4, 1);
		}
		uploadImage(target->image);
	}
}

void SDLCALL GPU_Blit(GPU_Image *image, GPU_Rect *src_rect, GPU_Target *target, float x, float y) {
	if (!nativeBlit(image, src_rect, target, x, y, 0.0f, 1.0f, 1.0f))
		cpuBlit(image, src_rect, target, x, y, 0.0f, 1.0f, 1.0f);
}

void SDLCALL GPU_BlitRotate(GPU_Image *image, GPU_Rect *src_rect, GPU_Target *target, float x, float y, float degrees) {
	if (!nativeBlit(image, src_rect, target, x, y, degrees, 1.0f, 1.0f))
		cpuBlit(image, src_rect, target, x, y, degrees, 1.0f, 1.0f);
}

void SDLCALL GPU_BlitScale(GPU_Image *image, GPU_Rect *src_rect, GPU_Target *target, float x, float y, float scaleX, float scaleY) {
	if (!nativeBlit(image, src_rect, target, x, y, 0.0f, scaleX, scaleY))
		cpuBlit(image, src_rect, target, x, y, 0.0f, scaleX, scaleY);
}

void SDLCALL GPU_BlitTransform(GPU_Image *image, GPU_Rect *src_rect, GPU_Target *target, float x, float y, float degrees, float scaleX, float scaleY) {
	if (!nativeBlit(image, src_rect, target, x, y, degrees, scaleX, scaleY))
		cpuBlit(image, src_rect, target, x, y, degrees, scaleX, scaleY);
}

void SDLCALL GPU_TriangleBatch(GPU_Image *image, GPU_Target *target, unsigned short num_vertices, float *values,
                               unsigned int num_indices, unsigned short *indices, GPU_BatchFlagEnum flags) {
	if (!image || !values || !indices || num_vertices == 0 || num_indices == 0)
		return;

	const bool xyz = (flags & GPU_BATCH_XYZ) == GPU_BATCH_XYZ;
	const bool st  = (flags & GPU_BATCH_ST) == GPU_BATCH_ST;
	const int stride = (xyz ? 3 : 2) + (st ? 2 : 0);
	if (!st || stride <= 0) {
		setUnsupported("GPU_TriangleBatch without texture coordinates");
		return;
	}

	std::vector<SDL3GPUVertex> vertices(num_vertices);
	const SDL_Color color = image->color;
	const float r = color.r / 255.0f;
	const float g = color.g / 255.0f;
	const float b = color.b / 255.0f;
	const float a = color.a / 255.0f;
	for (unsigned short i = 0; i < num_vertices; ++i) {
		const float *src = values + i * stride;
		vertices[i].x = src[0];
		vertices[i].y = src[1];
		vertices[i].r = r;
		vertices[i].g = g;
		vertices[i].b = b;
		vertices[i].a = a;
		const int uvOffset = xyz ? 3 : 2;
		vertices[i].s = src[uvOffset];
		vertices[i].t = src[uvOffset + 1];
	}

	if (!renderNativeIndexedTriangles(image, target, vertices.data(), num_vertices, indices, num_indices))
		setUnsupported("GPU_TriangleBatch native draw");
}

void SDLCALL GPU_FlushBlitBuffer(void) {}

void SDLCALL GPU_Flip(GPU_Target *target) {
	if (!rendererState.device || !rendererState.window)
		return;
	if (!target)
		target = rendererState.current_context_target;
	if (!ensureTargetBacking(target))
		return;

	SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(rendererState.device);
	if (!commands)
		return;

	SDL_GPUTexture *swapchainTexture = nullptr;
	Uint32 width = 0;
	Uint32 height = 0;
	if (!SDL_AcquireGPUSwapchainTexture(commands, rendererState.window, &swapchainTexture, &width, &height) || !swapchainTexture) {
		SDL_CancelGPUCommandBuffer(commands);
		return;
	}

	presentTarget(target, commands, swapchainTexture, width, height);
	SDL_SubmitGPUCommandBuffer(commands);
}

void SDLCALL GPU_RectangleFilled2(GPU_Target *target, GPU_Rect rect, SDL_Color color) {
	if (!target || !ensureTargetBacking(target))
		return;
	ensureImagePixelsCurrent(target->image);

	const int x0 = std::max<int>(0, static_cast<int>(rect.x));
	const int y0 = std::max<int>(0, static_cast<int>(rect.y));
	const int x1 = std::min<int>(target->image->w, static_cast<int>(rect.x + rect.w));
	const int y1 = std::min<int>(target->image->h, static_cast<int>(rect.y + rect.h));

	for (int y = y0; y < y1; ++y) {
		auto *row = target->image->pixels.data() + y * target->image->pitch;
		for (int x = x0; x < x1; ++x)
			copyPixelRow(row + x * target->image->bytes_per_pixel, target->image->bytes_per_pixel, reinterpret_cast<const Uint8 *>(&color), 4, 1);
	}
	uploadImage(target->image);
}

Uint32 SDLCALL GPU_CompileShader_RW(GPU_ShaderEnum shader_type, SDL_RWops *shader_source, GPU_bool free_rwops) {
	if (!shader_source) {
		setShaderMessage("No shader source was provided");
		return 0;
	}

	std::string source;
	char buffer[4096];
	while (true) {
		const size_t read = onsRWread(shader_source, buffer, 1, sizeof(buffer));
		if (read == 0)
			break;
		source.append(buffer, read);
	}
	if (free_rwops)
		SDL_RWclose(shader_source);

	const SDL3GPUShaderKind kind = identifyShaderSource(shader_type, source);
	SDL3GPUShaderObject shaderObject{};
	shaderObject.type = shader_type;
	shaderObject.kind = kind;
	shaderObject.source = std::move(source);
	if (kind == SDL3GPUShaderKind::Unknown &&
	    !compileNativeExternalShader(shader_type, shaderObject.source, shaderObject))
		return 0;

	const Uint32 shader = nextShaderObject++;
	const bool native = shaderObject.nativeShader != nullptr;
	shaderObjects[shader] = std::move(shaderObject);
	if (native) {
		std::snprintf(shaderMessage, sizeof(shaderMessage), "Compiled SDL3 native shadercross shader");
	} else {
		std::snprintf(shaderMessage, sizeof(shaderMessage), "Compiled SDL3 compatibility shader: %s", shaderKindName(kind));
	}
	return shader;
}

Uint32 SDLCALL GPU_LinkShaders(Uint32 shader_object1, Uint32 shader_object2) {
	Uint32 objects[2]{shader_object1, shader_object2};
	return GPU_LinkManyShaders(objects, 2);
}

Uint32 SDLCALL GPU_LinkManyShaders(Uint32 *shader_objects, int count) {
	if (!shader_objects || count <= 0) {
		setShaderMessage("No shader objects were provided for linking");
		return 0;
	}

	SDL3GPUProgramObject program{};
	bool hasVertex = false;
	for (int i = 0; i < count; ++i) {
		const Uint32 shader = shader_objects[i];
		auto it = shaderObjects.find(shader);
		if (it == shaderObjects.end()) {
			setShaderMessage("Unknown shader object passed to SDL3 compatibility linker");
			return 0;
		}
		program.shaders.push_back(shader);
		if (it->second.type == GPU_VERTEX_SHADER) {
			hasVertex = true;
			if (it->second.nativeShader)
				program.nativeVertexShader = it->second.nativeShader;
		} else if (it->second.type == GPU_FRAGMENT_SHADER || it->second.type == GPU_PIXEL_SHADER) {
			if (it->second.nativeShader) {
				program.nativeFragmentShader = it->second.nativeShader;
				program.nativeFragmentResources = it->second.nativeResources;
				program.nativeUniforms = it->second.nativeUniforms;
			} else {
				program.kind = it->second.kind;
			}
		}
	}

	if (program.nativeFragmentShader) {
		if (!hasVertex) {
			setShaderMessage("SDL3 native shader program must link a vertex shader");
			return 0;
		}
		Uint32 registerCount = 0;
		for (size_t i = 0; i < program.nativeUniforms.size(); ++i) {
			const auto &uniform = program.nativeUniforms[i];
			program.nativeUniformLookup[uniform.name] = i;
			for (int element = 0; element < uniform.arraySize; ++element)
				program.nativeUniformLookup[indexedUniformName(uniform.name.c_str(), element)] = i;
			registerCount = std::max<Uint32>(registerCount, uniform.registerIndex + static_cast<Uint32>(uniform.arraySize));
		}
		program.nativeUniformRegisters.resize(registerCount);
		const Uint32 object = nextShaderObject++;
		programObjects[object] = std::move(program);
		std::snprintf(shaderMessage, sizeof(shaderMessage), "Linked SDL3 native shadercross program");
		return object;
	}

	if (!hasVertex || program.kind == SDL3GPUShaderKind::Unknown || program.kind == SDL3GPUShaderKind::DefaultVertex) {
		setShaderMessage("SDL3 compatibility shader program must link a known fragment shader with a vertex shader");
		return 0;
	}

	const Uint32 object = nextShaderObject++;
	programObjects[object] = std::move(program);
	std::snprintf(shaderMessage, sizeof(shaderMessage), "Linked SDL3 compatibility shader program: %s", shaderKindName(programObjects[object].kind));
	return object;
}

GPU_bool SDLCALL GPU_LinkShaderProgram(Uint32 program_object) {
	return program_object != 0 && programObjects.count(program_object) > 0;
}

void SDLCALL GPU_ActivateShaderProgram(Uint32 program_object, GPU_ShaderBlock *block) {
	if (program_object != 0 && programObjects.count(program_object) == 0) {
		setShaderMessage("Cannot activate unknown SDL3 compatibility shader program");
		return;
	}
	if (rendererState.current_context_target && rendererState.current_context_target->context) {
		rendererState.current_context_target->context->current_shader_program = program_object;
		if (block)
			rendererState.current_context_target->context->current_shader_block = *block;
	}
}

void SDLCALL GPU_DeactivateShaderProgram(void) {
	if (rendererState.current_context_target && rendererState.current_context_target->context)
		rendererState.current_context_target->context->current_shader_program = 0;
}

const char *SDLCALL GPU_GetShaderMessage(void) {
	return shaderMessage;
}

int SDLCALL GPU_GetUniformLocation(Uint32 program_object, const char *uniform_name) {
	if (!uniform_name || program_object == 0)
		return -1;
	auto programIt = programObjects.find(program_object);
	if (programIt == programObjects.end())
		return -1;

	auto &program = programIt->second;
	auto existing = program.uniformLocations.find(uniform_name);
	if (existing != program.uniformLocations.end())
		return existing->second;

	const int location = nextUniformLocation++;
	program.uniformLocations[uniform_name] = location;
	program.locationNames[location] = uniform_name;
	uniformLocationOwners[location] = program_object;
	return location;
}

GPU_ShaderBlock SDLCALL GPU_LoadShaderBlock(Uint32, const char *, const char *, const char *, const char *) {
	GPU_ShaderBlock block{};
	block.position_loc              = 0;
	block.texcoord_loc              = 1;
	block.color_loc                 = 2;
	block.modelViewProjection_loc   = 3;
	return block;
}

void SDLCALL GPU_SetShaderImage(GPU_Image *image, int, int image_unit) {
	auto *program = activeProgramObject();
	if (!program || image_unit < 0 || image_unit >= static_cast<int>(program->images.size()))
		return;
	program->images[static_cast<size_t>(image_unit)] = image;
}

void SDLCALL GPU_SetUniformi(int location, int value) {
	auto owner = uniformLocationOwners.find(location);
	if (owner == uniformLocationOwners.end())
		return;
	auto programIt = programObjects.find(owner->second);
	if (programIt == programObjects.end())
		return;
	auto nameIt = programIt->second.locationNames.find(location);
	if (nameIt == programIt->second.locationNames.end())
		return;

	SDL3GPUUniformValue uniform{};
	uniform.type = SDL3GPUUniformType::Int;
	uniform.intValue = value;
	uniform.values[0] = static_cast<float>(value);
	programIt->second.uniforms[nameIt->second] = uniform;
	updateNativeUniformRegister(programIt->second, nameIt->second, uniform);
}

void SDLCALL GPU_SetUniformf(int location, float value) {
	auto owner = uniformLocationOwners.find(location);
	if (owner == uniformLocationOwners.end())
		return;
	auto programIt = programObjects.find(owner->second);
	if (programIt == programObjects.end())
		return;
	auto nameIt = programIt->second.locationNames.find(location);
	if (nameIt == programIt->second.locationNames.end())
		return;

	SDL3GPUUniformValue uniform{};
	uniform.type = SDL3GPUUniformType::Float;
	uniform.values[0] = value;
	programIt->second.uniforms[nameIt->second] = uniform;
	updateNativeUniformRegister(programIt->second, nameIt->second, uniform);
}

void SDLCALL GPU_SetUniformfv(int location, int num_elements_per_value, int, float *values) {
	if (!values)
		return;
	auto owner = uniformLocationOwners.find(location);
	if (owner == uniformLocationOwners.end())
		return;
	auto programIt = programObjects.find(owner->second);
	if (programIt == programObjects.end())
		return;
	auto nameIt = programIt->second.locationNames.find(location);
	if (nameIt == programIt->second.locationNames.end())
		return;

	SDL3GPUUniformValue uniform{};
	uniform.type = SDL3GPUUniformType::FloatVec;
	uniform.components = std::max(1, std::min(4, num_elements_per_value));
	for (int i = 0; i < uniform.components; ++i)
		uniform.values[i] = values[i];
	programIt->second.uniforms[nameIt->second] = uniform;
	updateNativeUniformRegister(programIt->second, nameIt->second, uniform);
}

RenderDriverId GPUController::makeRendererIdSDL3GPU() {
	return GPU_MakeRendererID("SDL3_GPU", GPU_RENDERER_SDL3_GPU, 3, 0);
}

void GPUController::initRendererFlagsSDL3GPU() {
	if (render_to_self < 0)
		render_to_self = 0;
}

int GPUController::getImageFormatSDL3GPU(RenderImage *) {
	return current_renderer ? current_renderer->formatRGBA : GL_RGBA;
}

void GPUController::printBlitBufferStateSDL3GPU() {
	sendToLog(LogLevel::Info, "SDL3_GPU transition backend does not use the SDL2_gpu blit buffer.\n");
}

void GPUController::syncRendererStateSDL3GPU() {
	if (rendererState.device)
		SDL_WaitForGPUIdle(rendererState.device);
}

int GPUController::getMaxTextureSizeSDL3GPU() {
	return 8192;
}

#endif
