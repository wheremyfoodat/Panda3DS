#include "renderer_sw/renderer_sw.hpp"

RendererSw::RendererSw(GPU& gpu, const std::array<u32, regNum>& internalRegs, const std::array<u32, extRegNum>& externalRegs)
	: Renderer(gpu, internalRegs, externalRegs) {}
RendererSw::~RendererSw() {}

void RendererSw::reset() { printf("RendererSW: Unimplemented reset call\n"); }
void RendererSw::display() { printf("RendererSW: Unimplemented display call\n"); }

void RendererSw::initGraphicsContext(SDL_Window* window) { printf("RendererSW: Unimplemented initGraphicsContext call\n"); }
void RendererSw::clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) { printf("RendererSW: Unimplemented clearBuffer call\n"); }

void RendererSw::displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) {
	printf("RendererSW: Unimplemented displayTransfer call\n");
}

void RendererSw::textureCopy(u32 inputAddr, u32 outputAddr, u32 totalBytes, u32 inputSize, u32 outputSize, u32 flags) {
	printf("RendererSW: Unimplemented textureCopy call\n");
}

void RendererSw::drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) {
	printf("RendererSW: Unimplemented drawVertices call\n");
}

void RendererSw::screenshot(const std::string& name) { printf("RendererSW: Unimplemented screenshot call\n"); }
