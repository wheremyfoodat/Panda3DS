#include "renderer.hpp"

#include <algorithm>
#include <unordered_map>

#include "PICA/gpu.hpp"

Renderer::Renderer(GPU& gpu, const std::array<u32, regNum>& internalRegs, const std::array<u32, extRegNum>& externalRegs)
	: gpu(gpu), regs(internalRegs), externalRegs(externalRegs) {}
Renderer::~Renderer() {}

std::optional<RendererType> Renderer::typeFromString(std::string inString) {
	// Transform to lower-case to make the setting case-insensitive
	std::transform(inString.begin(), inString.end(), inString.begin(), [](unsigned char c) { return std::tolower(c); });

	// Huge table of possible names and misspellings
	// Please stop misspelling Vulkan as Vulcan
	static const std::unordered_map<std::string, RendererType> map = {
		{"null", RendererType::Null},         {"nil", RendererType::Null},      {"none", RendererType::Null},
		{"gl", RendererType::OpenGL},         {"ogl", RendererType::OpenGL},    {"opengl", RendererType::OpenGL},
		{"vk", RendererType::Vulkan},         {"vulkan", RendererType::Vulkan}, {"vulcan", RendererType::Vulkan},
		{"mtl", RendererType::Metal},         {"metal", RendererType::Metal},
		{"sw", RendererType::Software},       {"soft", RendererType::Software}, {"software", RendererType::Software},
		{"softrast", RendererType::Software},
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
		case RendererType::Metal: return "metal";
		case RendererType::Software: return "software";
		default: return "Invalid";
	}
}

void Renderer::doSoftwareTextureCopy(u32 inputAddr, u32 outputAddr, u32 copySize, u32 inputWidth, u32 inputGap, u32 outputWidth, u32 outputGap) {
	u8* inputPointer = gpu.getPointerPhys<u8>(inputAddr);
	u8* outputPointer = gpu.getPointerPhys<u8>(outputAddr);

	if (inputPointer == nullptr || outputPointer == nullptr) {
		return;
	}

	u32 inputBytesLeft = inputWidth;
	u32 outputBytesLeft = outputWidth;
	u32 copyBytesLeft = copySize;

	while (copyBytesLeft > 0) {
		const u32 bytes = std::min<u32>({inputBytesLeft, outputBytesLeft, copyBytesLeft});
		std::memcpy(outputPointer, inputPointer, bytes);

		inputPointer += bytes;
		outputPointer += bytes;

		inputBytesLeft -= bytes;
		outputBytesLeft -= bytes;
		copyBytesLeft -= bytes;

		// Apply input and output gap when an input or output line ends
		if (inputBytesLeft == 0) {
			inputBytesLeft = inputWidth;
			inputPointer += inputGap;
		}

		if (outputBytesLeft == 0) {
			outputBytesLeft = outputWidth;
			outputPointer += outputGap;
		}
	}
}
