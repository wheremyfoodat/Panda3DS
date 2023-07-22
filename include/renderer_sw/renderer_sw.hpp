#include "renderer.hpp"

class GPU;

class RendererSw final : public Renderer {
  public:
	RendererSw(GPU& gpu, const std::array<u32, regNum>& internalRegs);
	~RendererSw() override;

	void reset() override;
	void display() override;
	void initGraphicsContext() override;
	void clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) override;
	void displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) override;
	void drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) override;
	void screenshot(const std::string& name) override;
};