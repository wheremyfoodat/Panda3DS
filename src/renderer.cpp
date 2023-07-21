#include "renderer.hpp"

#include <algorithm>
#include <unordered_map>

Renderer::Renderer(GPU& gpu, const std::array<u32, regNum>& internalRegs) : gpu(gpu), regs(internalRegs) {}
Renderer::~Renderer() {}

std::optional<RendererType> Renderer::typeFromString(std::string inString) {
	// Transform to lower-case to make the setting case-insensitive
	std::transform(inString.begin(), inString.end(), inString.begin(), [](unsigned char c) { return std::tolower(c); });

	// Huge table of possible names and misspellings
	// Please stop misspelling Vulkan as Vulcan
	static const std::unordered_map<std::string, RendererType> map = {
		{"null", RendererType::Null}, {"nil", RendererType::Null},      {"none", RendererType::Null},
		{"gl", RendererType::OpenGL}, {"ogl", RendererType::OpenGL},    {"opengl", RendererType::OpenGL},
		{"vk", RendererType::Vulkan}, {"vulkan", RendererType::Vulkan}, {"vulcan", RendererType::Vulkan},
	};

	if (auto search = map.find(inString); search != map.end()) {
		return search->second;
	}

	return std::nullopt;
}

const char* Renderer::typeToString(RendererType rendererType) {
	switch (rendererType) {
		case RendererType::Null: return "null";
		case RendererType::OpenGL: return "opengl";
		case RendererType::Vulkan: return "vulkan";
		default: return "Invalid";
	}
}