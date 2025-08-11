#include "renderer.hpp"

class GPU;

class RendererNull final : public Renderer {
  public:
	RendererNull(GPU& gpu, const std::array<u32, regNum>& internalRegs, const std::array<u32, extRegNum>& externalRegs);
	~RendererNull() override;

	void reset() override;
	void display() override;
	void initGraphicsContext(void* context) override;
	void clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) override;
	void displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) override;
	void textureCopy(u32 inputAddr, u32 outputAddr, u32 totalBytes, u32 inputSize, u32 outputSize, u32 flags) override;
	void drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) override;
	void screenshot(const std::string& name) override;
	void deinitGraphicsContext() override;

	// Tell the GPU core that we'll handle vertex fetch & shader execution in the renderer in order to speed up execution.
	// Of course, we don't do this and geometry is never actually processed, since this is the null renderer.
	virtual bool prepareForDraw(ShaderUnit& shaderUnit, PICA::DrawAcceleration* accel) override { return true; };
};
