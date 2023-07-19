#include "renderer_vk/renderer_vk.hpp"

RendererVK::RendererVK(GPU& gpu, const std::array<u32, regNum>& internalRegs) : Renderer(gpu, internalRegs) {}

RendererVK::~RendererVK() {}

void RendererVK::reset() {}

void RendererVK::display() {}

void RendererVK::initGraphicsContext() { static vk::DynamicLoader dl; }

void RendererVK::clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) {}

void RendererVK::displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) {}

void RendererVK::drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) {}

void RendererVK::screenshot(const std::string& name) {}