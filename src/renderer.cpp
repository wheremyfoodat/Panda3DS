#include "renderer.hpp"

#include <algorithm>

Renderer::Renderer(GPU& gpu, const std::array<u32, regNum>& internalRegs) : gpu(gpu), regs(internalRegs) {}
Renderer::~Renderer() {}

std::optional<RendererType> Renderer::typeFromString(std::string inString) {
	// case-insensitive
	std::transform(inString.begin(), inString.end(), inString.begin(), [](unsigned char c) { return std::tolower(c); });

	if (inString == "null")
		return RendererType::Null;
	else if (inString == "opengl")
		return RendererType::OpenGL;

	return std::nullopt;
}

std::string Renderer::typeToString(RendererType rendererType) {
	switch (rendererType) {
		case RendererType::Null: return "null";
		case RendererType::OpenGL: return "opengl";
		case RendererType::Vulkan: return "vk";
		default: return "Invalid";
	}
}