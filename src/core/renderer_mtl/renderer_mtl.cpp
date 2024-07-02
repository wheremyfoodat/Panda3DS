#include "renderer_mtl/renderer_mtl.hpp"

RendererMTL::RendererMTL(GPU& gpu, const std::array<u32, regNum>& internalRegs, const std::array<u32, extRegNum>& externalRegs)
	: Renderer(gpu, internalRegs, externalRegs) {}
RendererMTL::~RendererMTL() {}

void RendererMTL::reset() {
	// TODO: implement
}

void RendererMTL::display() {
	// TODO: implement
}

void RendererMTL::initGraphicsContext(SDL_Window* window) {
	// TODO: implement
}

void RendererMTL::clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) {
	// TODO: implement
}

void RendererMTL::displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) {
	// TODO: implement
}

void RendererMTL::textureCopy(u32 inputAddr, u32 outputAddr, u32 totalBytes, u32 inputSize, u32 outputSize, u32 flags) {
	// TODO: implement
}

void RendererMTL::drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) {
	// TODO: implement
}

void RendererMTL::screenshot(const std::string& name) {
	// TODO: implement
}

void RendererMTL::deinitGraphicsContext() {
	// TODO: implement
}
