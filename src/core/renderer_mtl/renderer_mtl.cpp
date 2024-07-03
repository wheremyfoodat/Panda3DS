#include "PICA/gpu.hpp"
#include "renderer_mtl/renderer_mtl.hpp"

#include <cmrc/cmrc.hpp>
#include <cstddef>

#include "SDL_metal.h"

using namespace PICA;

CMRC_DECLARE(RendererMTL);

// HACK: redefinition...
PICA::ColorFmt ToColorFormat(u32 format) {
	switch (format) {
		case 2: return PICA::ColorFmt::RGB565;
		case 3: return PICA::ColorFmt::RGBA5551;
		default: return static_cast<PICA::ColorFmt>(format);
	}
}

RendererMTL::RendererMTL(GPU& gpu, const std::array<u32, regNum>& internalRegs, const std::array<u32, extRegNum>& externalRegs)
	: Renderer(gpu, internalRegs, externalRegs) {}
RendererMTL::~RendererMTL() {}

void RendererMTL::reset() {
	colorRenderTargetCache.reset();
	depthStencilRenderTargetCache.reset();
	textureCache.reset();

	// TODO: implement
	Helpers::warn("RendererMTL::reset not implemented");
}

void RendererMTL::display() {
	CA::MetalDrawable* drawable = metalLayer->nextDrawable();

	MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
	MTL::RenderPassColorAttachmentDescriptor* colorAttachment = renderPassDescriptor->colorAttachments()->object(0);
	colorAttachment->setTexture(drawable->texture());
	colorAttachment->setLoadAction(MTL::LoadActionClear);
	colorAttachment->setClearColor(MTL::ClearColor{0.0f, 0.0f, 0.0f, 1.0f});
	colorAttachment->setStoreAction(MTL::StoreActionStore);

	beginRenderPassIfNeeded(renderPassDescriptor, drawable->texture());
	renderCommandEncoder->setRenderPipelineState(displayPipeline);
	renderCommandEncoder->setFragmentSamplerState(basicSampler, 0);

	using namespace PICA::ExternalRegs;

	// Top screen
	{
		const u32 topActiveFb = externalRegs[Framebuffer0Select] & 1;
		const u32 topScreenAddr = externalRegs[topActiveFb == 0 ? Framebuffer0AFirstAddr : Framebuffer0ASecondAddr];
		auto topScreen = colorRenderTargetCache.findFromAddress(topScreenAddr);

		if (topScreen) {
			renderCommandEncoder->setViewport(MTL::Viewport{0, 0, 400, 240, 0.0f, 1.0f});
			renderCommandEncoder->setFragmentTexture(topScreen->get().texture, 0);
			renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, NS::UInteger(0), NS::UInteger(4));
		}
	}

	// Bottom screen
	{
		const u32 bottomActiveFb = externalRegs[Framebuffer1Select] & 1;
		const u32 bottomScreenAddr = externalRegs[bottomActiveFb == 0 ? Framebuffer1AFirstAddr : Framebuffer1ASecondAddr];
		auto bottomScreen = colorRenderTargetCache.findFromAddress(bottomScreenAddr);

		if (bottomScreen) {
			renderCommandEncoder->setViewport(MTL::Viewport{40, 240, 320, 240, 0.0f, 1.0f});
			renderCommandEncoder->setFragmentTexture(bottomScreen->get().texture, 0);
			renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, NS::UInteger(0), NS::UInteger(4));
		}
	}

	endRenderPass();

	commandBuffer->presentDrawable(drawable);
	commitCommandBuffer();

	// Inform the vertex buffer cache that the frame ended
	vertexBufferCache.endFrame();
}

void RendererMTL::initGraphicsContext(SDL_Window* window) {
	// TODO: what should be the type of the view?
	void* view = SDL_Metal_CreateView(window);
	metalLayer = (CA::MetalLayer*)SDL_Metal_GetLayer(view);
	device = MTL::CreateSystemDefaultDevice();
	metalLayer->setDevice(device);
	commandQueue = device->newCommandQueue();

	// Helpers
	MTL::SamplerDescriptor* samplerDescriptor = MTL::SamplerDescriptor::alloc()->init();
	basicSampler = device->newSamplerState(samplerDescriptor);

	// -------- Pipelines --------

	// Load shaders
	auto mtlResources = cmrc::RendererMTL::get_filesystem();
	auto shaderSource = mtlResources.open("metal_shaders.metal");
	std::string source(shaderSource.begin(), shaderSource.size());
	MTL::CompileOptions* compileOptions = MTL::CompileOptions::alloc()->init();
	NS::Error* error = nullptr;
	MTL::Library* library = device->newLibrary(NS::String::string(source.c_str(), NS::ASCIIStringEncoding), compileOptions, &error);
	if (error) {
		Helpers::panic("Error loading shaders: %s", error->description()->cString(NS::ASCIIStringEncoding));
	}

	// Display
	MTL::Function* vertexDisplayFunction = library->newFunction(NS::String::string("vertexDisplay", NS::ASCIIStringEncoding));
	MTL::Function* fragmentDisplayFunction = library->newFunction(NS::String::string("fragmentDisplay", NS::ASCIIStringEncoding));

	MTL::RenderPipelineDescriptor* displayPipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
	displayPipelineDescriptor->setVertexFunction(vertexDisplayFunction);
	displayPipelineDescriptor->setFragmentFunction(fragmentDisplayFunction);
	auto* displayColorAttachment = displayPipelineDescriptor->colorAttachments()->object(0);
	displayColorAttachment->setPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm);

	error = nullptr;
	displayPipeline = device->newRenderPipelineState(displayPipelineDescriptor, &error);
	if (error) {
		Helpers::panic("Error creating display pipeline state: %s", error->description()->cString(NS::ASCIIStringEncoding));
	}

	// Blit
	MTL::Function* vertexBlitFunction = library->newFunction(NS::String::string("vertexBlit", NS::ASCIIStringEncoding));
	MTL::Function* fragmentBlitFunction = library->newFunction(NS::String::string("fragmentBlit", NS::ASCIIStringEncoding));

	blitPipelineCache.set(device, vertexBlitFunction, fragmentBlitFunction, nullptr);

	// Draw
	MTL::Function* vertexDrawFunction = library->newFunction(NS::String::string("vertexDraw", NS::ASCIIStringEncoding));
	MTL::Function* fragmentDrawFunction = library->newFunction(NS::String::string("fragmentDraw", NS::ASCIIStringEncoding));

	// -------- Vertex descriptor --------
	MTL::VertexDescriptor* vertexDescriptor = MTL::VertexDescriptor::alloc()->init();

	// Position
	MTL::VertexAttributeDescriptor* positionAttribute = vertexDescriptor->attributes()->object(0);
	positionAttribute->setFormat(MTL::VertexFormatFloat4);
	positionAttribute->setOffset(offsetof(Vertex, s.positions));
	positionAttribute->setBufferIndex(VERTEX_BUFFER_BINDING_INDEX);

	// Quaternion
	MTL::VertexAttributeDescriptor* quaternionAttribute = vertexDescriptor->attributes()->object(1);
	quaternionAttribute->setFormat(MTL::VertexFormatFloat4);
	quaternionAttribute->setOffset(offsetof(Vertex, s.quaternion));
	quaternionAttribute->setBufferIndex(VERTEX_BUFFER_BINDING_INDEX);

	// Color
	MTL::VertexAttributeDescriptor* colorAttribute = vertexDescriptor->attributes()->object(2);
	colorAttribute->setFormat(MTL::VertexFormatFloat4);
	colorAttribute->setOffset(offsetof(Vertex, s.colour));
	colorAttribute->setBufferIndex(VERTEX_BUFFER_BINDING_INDEX);

	// Texture coordinate 0
	MTL::VertexAttributeDescriptor* texCoord0Attribute = vertexDescriptor->attributes()->object(3);
	texCoord0Attribute->setFormat(MTL::VertexFormatFloat2);
	texCoord0Attribute->setOffset(offsetof(Vertex, s.texcoord0));
	texCoord0Attribute->setBufferIndex(VERTEX_BUFFER_BINDING_INDEX);

	// Texture coordinate 1
	MTL::VertexAttributeDescriptor* texCoord1Attribute = vertexDescriptor->attributes()->object(4);
	texCoord1Attribute->setFormat(MTL::VertexFormatFloat2);
	texCoord1Attribute->setOffset(offsetof(Vertex, s.texcoord1));
	texCoord1Attribute->setBufferIndex(VERTEX_BUFFER_BINDING_INDEX);

	// Texture coordinate 0 W
	MTL::VertexAttributeDescriptor* texCoord0WAttribute = vertexDescriptor->attributes()->object(5);
	texCoord0WAttribute->setFormat(MTL::VertexFormatFloat);
	texCoord0WAttribute->setOffset(offsetof(Vertex, s.texcoord0_w));
	texCoord0WAttribute->setBufferIndex(VERTEX_BUFFER_BINDING_INDEX);

	// View
	MTL::VertexAttributeDescriptor* viewAttribute = vertexDescriptor->attributes()->object(6);
	viewAttribute->setFormat(MTL::VertexFormatFloat3);
	viewAttribute->setOffset(offsetof(Vertex, s.view));
	viewAttribute->setBufferIndex(VERTEX_BUFFER_BINDING_INDEX);

	// Texture coordinate 2
	MTL::VertexAttributeDescriptor* texCoord2Attribute = vertexDescriptor->attributes()->object(7);
	texCoord2Attribute->setFormat(MTL::VertexFormatFloat2);
	texCoord2Attribute->setOffset(offsetof(Vertex, s.texcoord2));
	texCoord2Attribute->setBufferIndex(VERTEX_BUFFER_BINDING_INDEX);

	MTL::VertexBufferLayoutDescriptor* vertexBufferLayout = vertexDescriptor->layouts()->object(VERTEX_BUFFER_BINDING_INDEX);
	vertexBufferLayout->setStride(sizeof(Vertex));
	vertexBufferLayout->setStepFunction(MTL::VertexStepFunctionPerVertex);
	vertexBufferLayout->setStepRate(1);

	drawPipelineCache.set(device, vertexDrawFunction, fragmentDrawFunction, vertexDescriptor);

	// Depth stencil cache
	depthStencilCache.set(device);

	// Vertex buffer cache
	vertexBufferCache.set(device);
}

void RendererMTL::clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) {
	const auto color = colorRenderTargetCache.findFromAddress(startAddress);
	if (color) {
		const float r = Helpers::getBits<24, 8>(value) / 255.0f;
		const float g = Helpers::getBits<16, 8>(value) / 255.0f;
		const float b = Helpers::getBits<8, 8>(value) / 255.0f;
		const float a = (value & 0xff) / 255.0f;

		MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
		MTL::RenderPassColorAttachmentDescriptor* colorAttachment = renderPassDescriptor->colorAttachments()->object(0);
		colorAttachment->setTexture(color->get().texture);
		colorAttachment->setClearColor(MTL::ClearColor(r, g, b, a));
		colorAttachment->setLoadAction(MTL::LoadActionClear);
		colorAttachment->setStoreAction(MTL::StoreActionStore);

		beginRenderPassIfNeeded(renderPassDescriptor, color->get().texture);

		return;
	}

	const auto depth = depthStencilRenderTargetCache.findFromAddress(startAddress);
	if (depth) {
		float depthVal;
		const auto format = depth->get().format;
		if (format == DepthFmt::Depth16) {
			depthVal = (value & 0xffff) / 65535.0f;
		} else {
			depthVal = (value & 0xffffff) / 16777215.0f;
		}

		MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
  		MTL::RenderPassDepthAttachmentDescriptor* depthAttachment = renderPassDescriptor->depthAttachment();
        depthAttachment->setTexture(depth->get().texture);
        depthAttachment->setClearDepth(depthVal);
        depthAttachment->setLoadAction(MTL::LoadActionClear);
        depthAttachment->setStoreAction(MTL::StoreActionStore);

		if (format == DepthFmt::Depth24Stencil8) {
		    MTL::RenderPassStencilAttachmentDescriptor* stencilAttachment = renderPassDescriptor->stencilAttachment();
			stencilAttachment->setTexture(depth->get().texture);
			stencilAttachment->setClearStencil((value >> 24) & 0xff);
			stencilAttachment->setLoadAction(MTL::LoadActionClear);
			stencilAttachment->setStoreAction(MTL::StoreActionStore);
		}

		beginRenderPassIfNeeded(renderPassDescriptor, nullptr, depth->get().texture);

		return;
	}

	Helpers::warn("[RendererMTL::ClearBuffer] No buffer found!\n");
}

void RendererMTL::displayTransfer(u32 inputAddr, u32 outputAddr, u32 inputSize, u32 outputSize, u32 flags) {
	const u32 inputWidth = inputSize & 0xffff;
	const u32 inputHeight = inputSize >> 16;
	const auto inputFormat = ToColorFormat(Helpers::getBits<8, 3>(flags));
	const auto outputFormat = ToColorFormat(Helpers::getBits<12, 3>(flags));
	const bool verticalFlip = flags & 1;
	const PICA::Scaling scaling = static_cast<PICA::Scaling>(Helpers::getBits<24, 2>(flags));

	u32 outputWidth = outputSize & 0xffff;
	u32 outputHeight = outputSize >> 16;

	auto srcFramebuffer = getColorRenderTarget(inputAddr, inputFormat, inputWidth, outputHeight);
	Math::Rect<u32> srcRect = srcFramebuffer->getSubRect(inputAddr, outputWidth, outputHeight);

	if (verticalFlip) {
		std::swap(srcRect.bottom, srcRect.top);
	}

	// Apply scaling for the destination rectangle.
	if (scaling == PICA::Scaling::X || scaling == PICA::Scaling::XY) {
		outputWidth >>= 1;
	}

	if (scaling == PICA::Scaling::XY) {
		outputHeight >>= 1;
	}

	auto destFramebuffer = getColorRenderTarget(outputAddr, outputFormat, outputWidth, outputHeight);
	Math::Rect<u32> destRect = destFramebuffer->getSubRect(outputAddr, outputWidth, outputHeight);

	if (inputWidth != outputWidth) {
		// Helpers::warn("Strided display transfer is not handled correctly!\n");
	}

	// TODO: respect regions
	MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
	MTL::RenderPassColorAttachmentDescriptor* colorAttachment = renderPassDescriptor->colorAttachments()->object(0);
	colorAttachment->setTexture(destFramebuffer->texture);
	colorAttachment->setLoadAction(MTL::LoadActionClear);
	colorAttachment->setClearColor(MTL::ClearColor{0.0, 0.0, 0.0, 1.0});
	colorAttachment->setStoreAction(MTL::StoreActionStore);

	// Pipeline
	Metal::PipelineHash hash{destFramebuffer->format, DepthFmt::Unknown1};
	auto blitPipeline = blitPipelineCache.get(hash);

	beginRenderPassIfNeeded(renderPassDescriptor, destFramebuffer->texture);
	renderCommandEncoder->setRenderPipelineState(blitPipeline);
	renderCommandEncoder->setFragmentTexture(srcFramebuffer->texture, 0);
	renderCommandEncoder->setFragmentSamplerState(basicSampler, 0);

	renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, NS::UInteger(0), NS::UInteger(4));
}

void RendererMTL::textureCopy(u32 inputAddr, u32 outputAddr, u32 totalBytes, u32 inputSize, u32 outputSize, u32 flags) {
	// TODO: implement
	Helpers::warn("RendererMTL::textureCopy not implemented");
}

void RendererMTL::drawVertices(PICA::PrimType primType, std::span<const PICA::Vertex> vertices) {
	// Color
	auto colorRenderTarget = getColorRenderTarget(colourBufferLoc, colourBufferFormat, fbSize[0], fbSize[1]);

	// Depth stencil
	const u32 depthControl = regs[PICA::InternalRegs::DepthAndColorMask];
	const bool depthStencilWrite = regs[PICA::InternalRegs::DepthBufferWrite];
	const bool depthEnable = depthControl & 0x1;
	const bool depthWriteEnable = Helpers::getBit<12>(depthControl);
	const u8 depthFunc = Helpers::getBits<4, 3>(depthControl);
	const u8 colorMask = Helpers::getBits<8, 4>(depthControl);
	// TODO: color mask
	// gl.setColourMask(colorMask & 0x1, colorMask & 0x2, colorMask & 0x4, colorMask & 0x8);

	Metal::DepthStencilHash depthStencilHash{false, 1};
	depthStencilHash.stencilConfig = regs[PICA::InternalRegs::StencilTest];
	depthStencilHash.stencilOpConfig = regs[PICA::InternalRegs::StencilOp];
	const bool stencilEnable = Helpers::getBit<0>(depthStencilHash.stencilConfig);

	std::optional<Metal::DepthStencilRenderTarget> depthStencilRenderTarget = std::nullopt;
	if (depthEnable) {
		depthStencilHash.depthStencilWrite = depthWriteEnable && depthStencilWrite;
		depthStencilHash.depthFunc = depthFunc;
		depthStencilRenderTarget = getDepthRenderTarget();
	} else {
		if (depthWriteEnable) {
			depthStencilHash.depthStencilWrite = true;
			depthStencilRenderTarget = getDepthRenderTarget();
		} else if (stencilEnable) {
			depthStencilRenderTarget = getDepthRenderTarget();
		}
	}

	// -------- Pipeline --------
	Metal::PipelineHash pipelineHash{colorRenderTarget->format, DepthFmt::Unknown1};
	if (depthStencilRenderTarget) {
        pipelineHash.depthFmt = depthStencilRenderTarget->format;
    }

	// Blending
	pipelineHash.blendEnabled = (regs[PICA::InternalRegs::ColourOperation] & (1 << 8)) != 0;

	if (pipelineHash.blendEnabled) {
    	pipelineHash.blendControl = regs[PICA::InternalRegs::BlendFunc];
        // TODO: constant color
       	//pipelineHash.constantColor = regs[PICA::InternalRegs::BlendColour];
    	//const u8 r = pipelineHash.constantColor & 0xff;
    	//const u8 g = Helpers::getBits<8, 8>(pipelineHash.constantColor);
    	//const u8 b = Helpers::getBits<16, 8>(pipelineHash.constantColor);
    	//const u8 a = Helpers::getBits<24, 8>(pipelineHash.constantColor);
	}

	MTL::RenderPipelineState* pipeline = drawPipelineCache.get(pipelineHash);

	// Depth stencil state
	MTL::DepthStencilState* depthStencilState = depthStencilCache.get(depthStencilHash);

	// -------- Render --------
	MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
    MTL::RenderPassColorAttachmentDescriptor* colorAttachment = renderPassDescriptor->colorAttachments()->object(0);
    colorAttachment->setTexture(colorRenderTarget->texture);
    colorAttachment->setLoadAction(MTL::LoadActionLoad);
    colorAttachment->setStoreAction(MTL::StoreActionStore);
    if (depthStencilRenderTarget) {
        MTL::RenderPassDepthAttachmentDescriptor* depthAttachment = renderPassDescriptor->depthAttachment();
        depthAttachment->setTexture(depthStencilRenderTarget->texture);
        depthAttachment->setLoadAction(MTL::LoadActionLoad);
        depthAttachment->setStoreAction(MTL::StoreActionStore);
        if (depthStencilRenderTarget->format == DepthFmt::Depth24Stencil8) {
            MTL::RenderPassStencilAttachmentDescriptor* stencilAttachment = renderPassDescriptor->stencilAttachment();
            stencilAttachment->setTexture(depthStencilRenderTarget->texture);
            stencilAttachment->setLoadAction(MTL::LoadActionLoad);
            stencilAttachment->setStoreAction(MTL::StoreActionStore);
        }
    }

	beginRenderPassIfNeeded(renderPassDescriptor, colorRenderTarget->texture, (depthStencilRenderTarget ? depthStencilRenderTarget->texture : nullptr));
	renderCommandEncoder->setRenderPipelineState(pipeline);
	renderCommandEncoder->setDepthStencilState(depthStencilState);
	// If size is < 4KB, use inline vertex data, otherwise use a buffer
	if (vertices.size_bytes() < 4 * 1024) {
		renderCommandEncoder->setVertexBytes(vertices.data(), vertices.size_bytes(), VERTEX_BUFFER_BINDING_INDEX);
	} else {
	    Metal::BufferHandle buffer = vertexBufferCache.get(vertices);
		renderCommandEncoder->setVertexBuffer(buffer.buffer, buffer.offset, VERTEX_BUFFER_BINDING_INDEX);
	}

	// Bind resources
	setupTextureEnvState(renderCommandEncoder);
	bindTexturesToSlots(renderCommandEncoder);
	renderCommandEncoder->setVertexBytes(&regs[0x48], 0x200 - 0x48, 0);
	renderCommandEncoder->setFragmentBytes(&regs[0x48], 0x200 - 0x48, 0);

	// TODO: respect primitive type
	renderCommandEncoder->drawPrimitives(toMTLPrimitiveType(primType), NS::UInteger(0), NS::UInteger(vertices.size()));
}

void RendererMTL::screenshot(const std::string& name) {
	// TODO: implement
	Helpers::warn("RendererMTL::screenshot not implemented");
}

void RendererMTL::deinitGraphicsContext() {
	colorRenderTargetCache.reset();
	depthStencilRenderTargetCache.reset();
	textureCache.reset();

	// TODO: implement
	Helpers::warn("RendererMTL::deinitGraphicsContext not implemented");
}

std::optional<Metal::ColorRenderTarget> RendererMTL::getColorRenderTarget(
	u32 addr, PICA::ColorFmt format, u32 width, u32 height, bool createIfnotFound
) {
	// Try to find an already existing buffer that contains the provided address
	// This is a more relaxed check compared to getColourFBO as display transfer/texcopy may refer to
	// subrect of a surface and in case of texcopy we don't know the format of the surface.
	auto buffer = colorRenderTargetCache.findFromAddress(addr);
	if (buffer.has_value()) {
		return buffer.value().get();
	}

	if (!createIfnotFound) {
		return std::nullopt;
	}

	// Otherwise create and cache a new buffer.
	Metal::ColorRenderTarget sampleBuffer(device, addr, format, width, height);

	return colorRenderTargetCache.add(sampleBuffer);
}

Metal::DepthStencilRenderTarget& RendererMTL::getDepthRenderTarget() {
	Metal::DepthStencilRenderTarget sampleBuffer(device, depthBufferLoc, depthBufferFormat, fbSize[0], fbSize[1]);
	auto buffer = depthStencilRenderTargetCache.find(sampleBuffer);

	if (buffer.has_value()) {
		return buffer.value().get();
	} else {
		return depthStencilRenderTargetCache.add(sampleBuffer);
	}
}

MTL::Texture* RendererMTL::getTexture(Metal::Texture& tex) {
	auto buffer = textureCache.find(tex);

	if (buffer.has_value()) {
		return buffer.value().get().texture;
	} else {
		const auto textureData = std::span{gpu.getPointerPhys<u8>(tex.location), tex.sizeInBytes()};  // Get pointer to the texture data in 3DS memory
		Metal::Texture& newTex = textureCache.add(tex);
		newTex.decodeTexture(textureData);

		return newTex.texture;
	}
}

void RendererMTL::setupTextureEnvState(MTL::RenderCommandEncoder* encoder) {
	static constexpr std::array<u32, 6> ioBases = {
		PICA::InternalRegs::TexEnv0Source, PICA::InternalRegs::TexEnv1Source, PICA::InternalRegs::TexEnv2Source,
		PICA::InternalRegs::TexEnv3Source, PICA::InternalRegs::TexEnv4Source, PICA::InternalRegs::TexEnv5Source,
	};

	struct {
		u32 textureEnvSourceRegs[6];
		u32 textureEnvOperandRegs[6];
		u32 textureEnvCombinerRegs[6];
		u32 textureEnvScaleRegs[6];
	} envState;
	u32 textureEnvColourRegs[6];

	for (int i = 0; i < 6; i++) {
		const u32 ioBase = ioBases[i];

		envState.textureEnvSourceRegs[i] = regs[ioBase];
		envState.textureEnvOperandRegs[i] = regs[ioBase + 1];
		envState.textureEnvCombinerRegs[i] = regs[ioBase + 2];
		textureEnvColourRegs[i] = regs[ioBase + 3];
		envState.textureEnvScaleRegs[i] = regs[ioBase + 4];
	}

	encoder->setVertexBytes(&textureEnvColourRegs, sizeof(textureEnvColourRegs), 1);
	encoder->setFragmentBytes(&envState, sizeof(envState), 1);
}

void RendererMTL::bindTexturesToSlots(MTL::RenderCommandEncoder* encoder) {
	static constexpr std::array<u32, 3> ioBases = {
		PICA::InternalRegs::Tex0BorderColor,
		PICA::InternalRegs::Tex1BorderColor,
		PICA::InternalRegs::Tex2BorderColor,
	};

	for (int i = 0; i < 3; i++) {
		if ((regs[PICA::InternalRegs::TexUnitCfg] & (1 << i)) == 0) {
			continue;
		}

		const size_t ioBase = ioBases[i];

		const u32 dim = regs[ioBase + 1];
		const u32 config = regs[ioBase + 2];
		const u32 height = dim & 0x7ff;
		const u32 width = Helpers::getBits<16, 11>(dim);
		const u32 addr = (regs[ioBase + 4] & 0x0FFFFFFF) << 3;
		u32 format = regs[ioBase + (i == 0 ? 13 : 5)] & 0xF;

		if (addr != 0) [[likely]] {
			Metal::Texture targetTex(device, addr, static_cast<PICA::TextureFmt>(format), width, height, config);
			MTL::Texture* tex = getTexture(targetTex);
			encoder->setFragmentTexture(tex, i);
		} else {
			// TODO: bind a dummy texture?
		}
	}
}
