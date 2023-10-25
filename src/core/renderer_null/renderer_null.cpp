#include "renderer_null/renderer_null.hpp"

RendererNull::RendererNull(GPU& gpu, const std::array<u32, regNum>& internalRegs, const std::array<u32, extRegNum>& externalRegs)
	: Renderer(gpu, internalRegs, externalRegs) {}
RendererNull::~RendererNull() {}

void RendererNull::reset() {}
void RendererNull::display() {}
void RendererNull::initGraphicsContext(SDL_Window* window) {}
void RendererNull::clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) {}
void RendererNull::displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) {}
void RendererNull::textureCopy(u32 inputAddr, u32 outputAddr, u32 totalBytes, u32 inputSize, u32 outputSize, u32 flags) {}
void RendererNull::drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) {}
void RendererNull::screenshot(const std::string& name) {}
void RendererNull::deinitGraphicsContext() {}