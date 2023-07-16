#include "renderer.hpp"

Renderer::Renderer(GPU& gpu, const std::array<u32, regNum>& internalRegs) : gpu(gpu), regs(internalRegs) {}
Renderer::~Renderer() {}