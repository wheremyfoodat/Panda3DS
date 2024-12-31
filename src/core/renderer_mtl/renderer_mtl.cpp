#include "renderer_mtl/renderer_mtl.hpp"

#include <cmrc/cmrc.hpp>
#include <cstddef>

#include "renderer_mtl/mtl_lut_texture.hpp"

// Hack: Apple annoyingly defines a global "NO" macro which ends up conflicting with our own code...
#undef NO

#include "PICA/gpu.hpp"
#include "SDL_metal.h"

using namespace PICA;

CMRC_DECLARE(RendererMTL);

static constexpr u16 LIGHTING_LUT_TEXTURE_WIDTH = 256;
static constexpr u32 FOG_LUT_TEXTURE_WIDTH = 128;
// Bind the vertex buffer to binding 30 so that it doesn't occupy the lower indices
static constexpr uint VERTEX_BUFFER_BINDING_INDEX = 30;

// HACK: redefinition...
PICA::ColorFmt ToColorFormat(u32 format) {
	switch (format) {
		case 2: return PICA::ColorFmt::RGB565;
		case 3: return PICA::ColorFmt::RGBA5551;
		default: return static_cast<PICA::ColorFmt>(format);
	}
}

MTL::Library* loadLibrary(MTL::Device* device, const cmrc::file& shaderSource) {
	// MTL::CompileOptions* compileOptions = MTL::CompileOptions::alloc()->init();
	NS::Error* error = nullptr;
	MTL::Library* library = device->newLibrary(Metal::createDispatchData(shaderSource.begin(), shaderSource.size()), &error);
	// MTL::Library* library = device->newLibrary(NS::String::string(source.c_str(), NS::ASCIIStringEncoding), compileOptions, &error);
	if (error) {
		Helpers::panic("Error loading shaders: %s", error->description()->cString(NS::ASCIIStringEncoding));
	}

	return library;
}

RendererMTL::RendererMTL(GPU& gpu, const std::array<u32, regNum>& internalRegs, const std::array<u32, extRegNum>& externalRegs)
	: Renderer(gpu, internalRegs, externalRegs) {}

RendererMTL::~RendererMTL() {}

void RendererMTL::reset() {
	vertexBufferCache.reset();
	depthStencilCache.reset();
	drawPipelineCache.reset();
	blitPipelineCache.reset();
	textureCache.reset();
	depthStencilRenderTargetCache.reset();
	colorRenderTargetCache.reset();
}

void RendererMTL::display() {
	CA::MetalDrawable* drawable = metalLayer->nextDrawable();
	if (!drawable) {
		return;
	}

	using namespace PICA::ExternalRegs;

	// Top screen
	const u32 topActiveFb = externalRegs[Framebuffer0Select] & 1;
	const u32 topScreenAddr = externalRegs[topActiveFb == 0 ? Framebuffer0AFirstAddr : Framebuffer0ASecondAddr];
	auto topScreen = colorRenderTargetCache.findFromAddress(topScreenAddr);

	if (topScreen) {
		clearColor(nullptr, topScreen->get().texture);
	}

	// Bottom screen
	const u32 bottomActiveFb = externalRegs[Framebuffer1Select] & 1;
	const u32 bottomScreenAddr = externalRegs[bottomActiveFb == 0 ? Framebuffer1AFirstAddr : Framebuffer1ASecondAddr];
	auto bottomScreen = colorRenderTargetCache.findFromAddress(bottomScreenAddr);

	if (bottomScreen) {
		clearColor(nullptr, bottomScreen->get().texture);
	}

	// Draw
	commandBuffer->pushDebugGroup(toNSString("Display"));

	MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
	MTL::RenderPassColorAttachmentDescriptor* colorAttachment = renderPassDescriptor->colorAttachments()->object(0);
	colorAttachment->setTexture(drawable->texture());
	colorAttachment->setLoadAction(MTL::LoadActionClear);
	colorAttachment->setClearColor(MTL::ClearColor{0.0f, 0.0f, 0.0f, 1.0f});
	colorAttachment->setStoreAction(MTL::StoreActionStore);

	nextRenderPassName = "Display";
	beginRenderPassIfNeeded(renderPassDescriptor, false, drawable->texture());
	renderCommandEncoder->setRenderPipelineState(displayPipeline);
	renderCommandEncoder->setFragmentSamplerState(nearestSampler, 0);

	// Top screen
	if (topScreen) {
		renderCommandEncoder->setViewport(MTL::Viewport{0, 0, 400, 240, 0.0f, 1.0f});
		renderCommandEncoder->setFragmentTexture(topScreen->get().texture, 0);
		renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, NS::UInteger(0), NS::UInteger(4));
	}

	// Bottom screen
	if (bottomScreen) {
		renderCommandEncoder->setViewport(MTL::Viewport{40, 240, 320, 240, 0.0f, 1.0f});
		renderCommandEncoder->setFragmentTexture(bottomScreen->get().texture, 0);
		renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, NS::UInteger(0), NS::UInteger(4));
	}

	endRenderPass();

	commandBuffer->presentDrawable(drawable);
	commandBuffer->popDebugGroup();
	commitCommandBuffer();

	// Inform the vertex buffer cache that the frame ended
	vertexBufferCache.endFrame();

	// Release
	drawable->release();
}

void RendererMTL::initGraphicsContext(SDL_Window* window) {
	// TODO: what should be the type of the view?
	void* view = SDL_Metal_CreateView(window);
	metalLayer = (CA::MetalLayer*)SDL_Metal_GetLayer(view);
	device = MTL::CreateSystemDefaultDevice();
	metalLayer->setDevice(device);
	commandQueue = device->newCommandQueue();

	// Textures
	MTL::TextureDescriptor* textureDescriptor = MTL::TextureDescriptor::alloc()->init();
	textureDescriptor->setTextureType(MTL::TextureType2D);
	textureDescriptor->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
	textureDescriptor->setWidth(1);
	textureDescriptor->setHeight(1);
	textureDescriptor->setStorageMode(MTL::StorageModePrivate);
	textureDescriptor->setUsage(MTL::TextureUsageShaderRead);

	nullTexture = device->newTexture(textureDescriptor);
	nullTexture->setLabel(toNSString("Null texture"));
	textureDescriptor->release();

	// Samplers
	MTL::SamplerDescriptor* samplerDescriptor = MTL::SamplerDescriptor::alloc()->init();
	samplerDescriptor->setLabel(toNSString("Sampler (nearest)"));
	nearestSampler = device->newSamplerState(samplerDescriptor);

	samplerDescriptor->setMinFilter(MTL::SamplerMinMagFilterLinear);
	samplerDescriptor->setMagFilter(MTL::SamplerMinMagFilterLinear);
	samplerDescriptor->setLabel(toNSString("Sampler (linear)"));
	linearSampler = device->newSamplerState(samplerDescriptor);

	samplerDescriptor->release();

	lutLightingTexture = new Metal::LutTexture(
		device, MTL::TextureType2DArray, MTL::PixelFormatR16Unorm, LIGHTING_LUT_TEXTURE_WIDTH, Lights::LUT_Count, "Lighting LUT texture"
	);
	lutFogTexture = new Metal::LutTexture(device, MTL::TextureType1DArray, MTL::PixelFormatRG32Float, FOG_LUT_TEXTURE_WIDTH, 1, "Fog LUT texture");

	// -------- Pipelines --------

	// Load shaders
	auto mtlResources = cmrc::RendererMTL::get_filesystem();
	library = loadLibrary(device, mtlResources.open("metal_shaders.metallib"));
	MTL::Library* blitLibrary = loadLibrary(device, mtlResources.open("metal_blit.metallib"));
	// MTL::Library* copyToLutTextureLibrary = loadLibrary(device, mtlResources.open("metal_copy_to_lut_texture.metallib"));

	// Display
	MTL::Function* vertexDisplayFunction = library->newFunction(NS::String::string("vertexDisplay", NS::ASCIIStringEncoding));
	MTL::Function* fragmentDisplayFunction = library->newFunction(NS::String::string("fragmentDisplay", NS::ASCIIStringEncoding));

	MTL::RenderPipelineDescriptor* displayPipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
	displayPipelineDescriptor->setVertexFunction(vertexDisplayFunction);
	displayPipelineDescriptor->setFragmentFunction(fragmentDisplayFunction);
	auto* displayColorAttachment = displayPipelineDescriptor->colorAttachments()->object(0);
	displayColorAttachment->setPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm);

	NS::Error* error = nullptr;
	displayPipelineDescriptor->setLabel(toNSString("Display pipeline"));
	displayPipeline = device->newRenderPipelineState(displayPipelineDescriptor, &error);
	if (error) {
		Helpers::panic("Error creating display pipeline state: %s", error->description()->cString(NS::ASCIIStringEncoding));
	}
	displayPipelineDescriptor->release();
	vertexDisplayFunction->release();
	fragmentDisplayFunction->release();

	// Blit
	MTL::Function* vertexBlitFunction = blitLibrary->newFunction(NS::String::string("vertexBlit", NS::ASCIIStringEncoding));
	MTL::Function* fragmentBlitFunction = blitLibrary->newFunction(NS::String::string("fragmentBlit", NS::ASCIIStringEncoding));

	blitPipelineCache.set(device, vertexBlitFunction, fragmentBlitFunction);

	// Draw
	MTL::Function* vertexDrawFunction = library->newFunction(NS::String::string("vertexDraw", NS::ASCIIStringEncoding));

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

	drawPipelineCache.set(device, library, vertexDrawFunction, vertexDescriptor);

	// Copy to LUT texture
	/*
	MTL::FunctionConstantValues* constants = MTL::FunctionConstantValues::alloc()->init();
	constants->setConstantValue(&LIGHTING_LUT_TEXTURE_WIDTH, MTL::DataTypeUShort, NS::UInteger(0));

	error = nullptr;
	MTL::Function* vertexCopyToLutTextureFunction =
		copyToLutTextureLibrary->newFunction(NS::String::string("vertexCopyToLutTexture", NS::ASCIIStringEncoding), constants, &error);
	if (error) {
		Helpers::panic("Error creating copy_to_lut_texture vertex function: %s", error->description()->cString(NS::ASCIIStringEncoding));
	}
	constants->release();

	MTL::RenderPipelineDescriptor* copyToLutTexturePipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
	copyToLutTexturePipelineDescriptor->setVertexFunction(vertexCopyToLutTextureFunction);
	// Disable rasterization
	copyToLutTexturePipelineDescriptor->setRasterizationEnabled(false);

	error = nullptr;
	copyToLutTexturePipelineDescriptor->setLabel(toNSString("Copy to LUT texture pipeline"));
	copyToLutTexturePipeline = device->newRenderPipelineState(copyToLutTexturePipelineDescriptor, &error);
	if (error) {
		Helpers::panic("Error creating copy_to_lut_texture pipeline state: %s", error->description()->cString(NS::ASCIIStringEncoding));
	}
	copyToLutTexturePipelineDescriptor->release();
	vertexCopyToLutTextureFunction->release();
	*/

	// Depth stencil cache
	depthStencilCache.set(device);

	// Vertex buffer cache
	vertexBufferCache.set(device);

	// -------- Depth stencil state --------
	MTL::DepthStencilDescriptor* depthStencilDescriptor = MTL::DepthStencilDescriptor::alloc()->init();
	depthStencilDescriptor->setLabel(toNSString("Default depth stencil state"));
	defaultDepthStencilState = device->newDepthStencilState(depthStencilDescriptor);
	depthStencilDescriptor->release();

	blitLibrary->release();
	// copyToLutTextureLibrary->release();
}

void RendererMTL::clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control) {
	const auto color = colorRenderTargetCache.findFromAddress(startAddress);
	if (color) {
		const float r = Helpers::getBits<24, 8>(value) / 255.0f;
		const float g = Helpers::getBits<16, 8>(value) / 255.0f;
		const float b = Helpers::getBits<8, 8>(value) / 255.0f;
		const float a = (value & 0xff) / 255.0f;

		colorClearOps[color->get().texture] = {r, g, b, a};

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

		depthClearOps[depth->get().texture] = depthVal;

		if (format == DepthFmt::Depth24Stencil8) {
			const u8 stencilVal = value >> 24;
			stencilClearOps[depth->get().texture] = stencilVal;
		}

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
	nextRenderPassName = "Clear before display transfer";
	clearColor(nullptr, srcFramebuffer->texture);
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
	// TODO: clear if not blitting to the whole framebuffer
	Math::Rect<u32> destRect = destFramebuffer->getSubRect(outputAddr, outputWidth, outputHeight);

	if (inputWidth != outputWidth) {
		// Helpers::warn("Strided display transfer is not handled correctly!\n");
	}

	textureCopyImpl(*srcFramebuffer, *destFramebuffer, srcRect, destRect);
}

void RendererMTL::textureCopy(u32 inputAddr, u32 outputAddr, u32 totalBytes, u32 inputSize, u32 outputSize, u32 flags) {
	// Texture copy size is aligned to 16 byte units
	const u32 copySize = totalBytes & ~0xf;
	if (copySize == 0) {
		Helpers::warn("TextureCopy total bytes less than 16!\n");
		return;
	}

	// The width and gap are provided in 16-byte units.
	const u32 inputWidth = (inputSize & 0xffff) << 4;
	const u32 inputGap = (inputSize >> 16) << 4;
	const u32 outputWidth = (outputSize & 0xffff) << 4;
	const u32 outputGap = (outputSize >> 16) << 4;

	if (inputGap != 0 || outputGap != 0) {
		// Helpers::warn("Strided texture copy\n");
	}

	if (inputWidth != outputWidth) {
		Helpers::warn("Input width does not match output width, cannot accelerate texture copy!");
		return;
	}

	// Texture copy is a raw data copy in PICA, which means no format or tiling information is provided to the engine.
	// Depending if the target surface is linear or tiled, games set inputWidth to either the width of the texture or
	// the width multiplied by eight (because tiles are stored linearly in memory).
	// To properly accelerate this we must examine each surface individually. For now we assume the most common case
	// of tiled surface with RGBA8 format. If our assumption does not hold true, we abort the texture copy as inserting
	// that surface is not correct.

	// We assume the source surface is tiled and RGBA8. inputWidth is in bytes so divide it
	// by eight * sizePerPixel(RGBA8) to convert it to a useable width.
	const u32 bpp = sizePerPixel(PICA::ColorFmt::RGBA8);
	const u32 copyStride = (inputWidth + inputGap) / (8 * bpp);
	const u32 copyWidth = inputWidth / (8 * bpp);

	// inputHeight/outputHeight are typically set to zero so they cannot be used to get the height of the copy region
	// in contrast to display transfer. Compute height manually by dividing the copy size with the copy width. The result
	// is the number of vertical tiles so multiply that by eight to get the actual copy height.
	u32 copyHeight;
	if (inputWidth != 0) [[likely]] {
		copyHeight = (copySize / inputWidth) * 8;
	} else {
		copyHeight = 0;
	}

	// Find the source surface.
	auto srcFramebuffer = getColorRenderTarget(inputAddr, PICA::ColorFmt::RGBA8, copyStride, copyHeight, false);
	if (!srcFramebuffer) {
		Helpers::warn("RendererMTL::TextureCopy failed to locate src framebuffer!\n");
		return;
	}
	nextRenderPassName = "Clear before texture copy";
	clearColor(nullptr, srcFramebuffer->texture);

	Math::Rect<u32> srcRect = srcFramebuffer->getSubRect(inputAddr, copyWidth, copyHeight);

	// Assume the destination surface has the same format. Unless the surfaces have the same block width,
	// texture copy does not make sense.
	auto destFramebuffer = getColorRenderTarget(outputAddr, srcFramebuffer->format, copyWidth, copyHeight);
	// TODO: clear if not blitting to the whole framebuffer
	Math::Rect<u32> destRect = destFramebuffer->getSubRect(outputAddr, copyWidth, copyHeight);

	textureCopyImpl(*srcFramebuffer, *destFramebuffer, srcRect, destRect);
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

	Metal::DepthStencilHash depthStencilHash;
	depthStencilHash.stencilConfig = regs[PICA::InternalRegs::StencilTest];
	depthStencilHash.stencilOpConfig = regs[PICA::InternalRegs::StencilOp];
	depthStencilHash.depthStencilWrite = false;
	depthStencilHash.depthFunc = 1;
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

	// Depth uniforms
	struct {
		float depthScale;
		float depthOffset;
		bool depthMapEnable;
	} depthUniforms;
	depthUniforms.depthScale = Floats::f24::fromRaw(regs[PICA::InternalRegs::DepthScale] & 0xffffff).toFloat32();
	depthUniforms.depthOffset = Floats::f24::fromRaw(regs[PICA::InternalRegs::DepthOffset] & 0xffffff).toFloat32();
	depthUniforms.depthMapEnable = regs[PICA::InternalRegs::DepthmapEnable] & 1;

	// -------- Pipeline --------
	Metal::DrawPipelineHash pipelineHash;
	pipelineHash.colorFmt = colorRenderTarget->format;
	if (depthStencilRenderTarget) {
		pipelineHash.depthFmt = depthStencilRenderTarget->format;
	} else {
		pipelineHash.depthFmt = DepthFmt::Unknown1;
	}
	pipelineHash.fragHash.lightingEnabled = regs[0x008F] & 1;
	pipelineHash.fragHash.lightingNumLights = regs[0x01C2] & 0x7;
	pipelineHash.fragHash.lightingConfig1 = regs[0x01C4u];
	pipelineHash.fragHash.alphaControl = regs[0x104];

	// Blending and logic op
	pipelineHash.blendEnabled = (regs[PICA::InternalRegs::ColourOperation] & (1 << 8)) != 0;
	pipelineHash.colorWriteMask = colorMask;

	u8 logicOp = 3;  // Copy
	if (pipelineHash.blendEnabled) {
		pipelineHash.blendControl = regs[PICA::InternalRegs::BlendFunc];
	} else {
		logicOp = Helpers::getBits<0, 4>(regs[PICA::InternalRegs::LogicOp]);
	}

	MTL::RenderPipelineState* pipeline = drawPipelineCache.get(pipelineHash);

	// Depth stencil state
	MTL::DepthStencilState* depthStencilState = depthStencilCache.get(depthStencilHash);

	// -------- Render --------
	MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
	bool doesClear = clearColor(renderPassDescriptor, colorRenderTarget->texture);
	if (depthStencilRenderTarget) {
		if (clearDepth(renderPassDescriptor, depthStencilRenderTarget->texture)) doesClear = true;
		if (depthStencilRenderTarget->format == DepthFmt::Depth24Stencil8) {
			if (clearStencil(renderPassDescriptor, depthStencilRenderTarget->texture)) doesClear = true;
		}
	}

	nextRenderPassName = "Draw vertices";
	beginRenderPassIfNeeded(
		renderPassDescriptor, doesClear, colorRenderTarget->texture, (depthStencilRenderTarget ? depthStencilRenderTarget->texture : nullptr)
	);

	// Update the LUT texture if necessary
	if (gpu.lightingLUTDirty) {
		updateLightingLUT(renderCommandEncoder);
	}
	if (gpu.fogLUTDirty) {
		updateFogLUT(renderCommandEncoder);
	}

	commandEncoder.setRenderPipelineState(pipeline);
	commandEncoder.setDepthStencilState(depthStencilState);
	// If size is < 4KB, use inline vertex data, otherwise use a buffer
	if (vertices.size_bytes() < 4 * 1024) {
		renderCommandEncoder->setVertexBytes(vertices.data(), vertices.size_bytes(), VERTEX_BUFFER_BINDING_INDEX);
	} else {
		Metal::BufferHandle buffer = vertexBufferCache.get(vertices.data(), vertices.size_bytes());
		renderCommandEncoder->setVertexBuffer(buffer.buffer, buffer.offset, VERTEX_BUFFER_BINDING_INDEX);
	}

	// Viewport
	const u32 viewportX = regs[PICA::InternalRegs::ViewportXY] & 0x3ff;
	const u32 viewportY = (regs[PICA::InternalRegs::ViewportXY] >> 16) & 0x3ff;
	const u32 viewportWidth = Floats::f24::fromRaw(regs[PICA::InternalRegs::ViewportWidth] & 0xffffff).toFloat32() * 2.0f;
	const u32 viewportHeight = Floats::f24::fromRaw(regs[PICA::InternalRegs::ViewportHeight] & 0xffffff).toFloat32() * 2.0f;
	const auto rect = colorRenderTarget->getSubRect(colourBufferLoc, fbSize[0], fbSize[1]);
	MTL::Viewport viewport{double(rect.left + viewportX), double(rect.bottom + viewportY), double(viewportWidth), double(viewportHeight), 0.0, 1.0};
	renderCommandEncoder->setViewport(viewport);

	// Blend color
	if (pipelineHash.blendEnabled) {
		u32 constantColor = regs[PICA::InternalRegs::BlendColour];
		const u8 r = constantColor & 0xff;
		const u8 g = Helpers::getBits<8, 8>(constantColor);
		const u8 b = Helpers::getBits<16, 8>(constantColor);
		const u8 a = Helpers::getBits<24, 8>(constantColor);

		renderCommandEncoder->setBlendColor(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
	}

	// Stencil reference
	if (stencilEnable) {
		const s8 reference = s8(Helpers::getBits<16, 8>(depthStencilHash.stencilConfig));  // Signed reference value
		renderCommandEncoder->setStencilReferenceValue(reference);
	}

	// Bind resources
	setupTextureEnvState(renderCommandEncoder);
	bindTexturesToSlots();
	renderCommandEncoder->setVertexBytes(&regs[0x48], (0x200 - 0x48) * sizeof(regs[0]), 0);
	renderCommandEncoder->setFragmentBytes(&regs[0x48], (0x200 - 0x48) * sizeof(regs[0]), 0);
	renderCommandEncoder->setVertexBytes(&depthUniforms, sizeof(depthUniforms), 2);
	renderCommandEncoder->setFragmentBytes(&logicOp, sizeof(logicOp), 2);
	u32 lutSlices[2] = {lutLightingTexture->getCurrentIndex(), lutFogTexture->getCurrentIndex()};
	renderCommandEncoder->setFragmentBytes(&lutSlices, sizeof(lutSlices), 3);

	renderCommandEncoder->drawPrimitives(toMTLPrimitiveType(primType), NS::UInteger(0), NS::UInteger(vertices.size()));
}

void RendererMTL::screenshot(const std::string& name) {
	// TODO: implement
	Helpers::warn("RendererMTL::screenshot not implemented");
}

void RendererMTL::deinitGraphicsContext() {
	reset();

	delete lutLightingTexture;
	delete lutFogTexture;

	// copyToLutTexturePipeline->release();
	displayPipeline->release();
	defaultDepthStencilState->release();
	nullTexture->release();
	linearSampler->release();
	nearestSampler->release();
	library->release();
	commandQueue->release();
	device->release();
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
	auto& colorBuffer = colorRenderTargetCache.add(sampleBuffer);

	// Clear the color buffer
	colorClearOps[colorBuffer.texture] = {0, 0, 0, 0};
	return colorBuffer;
}

Metal::DepthStencilRenderTarget& RendererMTL::getDepthRenderTarget() {
	Metal::DepthStencilRenderTarget sampleBuffer(device, depthBufferLoc, depthBufferFormat, fbSize[0], fbSize[1]);
	auto buffer = depthStencilRenderTargetCache.find(sampleBuffer);

	if (buffer.has_value()) {
		return buffer.value().get();
	} else {
		auto& depthBuffer = depthStencilRenderTargetCache.add(sampleBuffer);

		// Clear the depth buffer
		depthClearOps[depthBuffer.texture] = 0.0f;
		if (depthBuffer.format == DepthFmt::Depth24Stencil8) {
			stencilClearOps[depthBuffer.texture] = 0;
		}

		return depthBuffer;
	}
}

Metal::Texture& RendererMTL::getTexture(Metal::Texture& tex) {
	auto buffer = textureCache.find(tex);

	if (buffer.has_value()) {
		return buffer.value().get();
	} else {
		const auto textureData = std::span{gpu.getPointerPhys<u8>(tex.location), tex.sizeInBytes()};  // Get pointer to the texture data in 3DS memory
		Metal::Texture& newTex = textureCache.add(tex);
		newTex.decodeTexture(textureData);

		return newTex;
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

void RendererMTL::bindTexturesToSlots() {
	static constexpr std::array<u32, 3> ioBases = {
		PICA::InternalRegs::Tex0BorderColor,
		PICA::InternalRegs::Tex1BorderColor,
		PICA::InternalRegs::Tex2BorderColor,
	};

	for (int i = 0; i < 3; i++) {
		if ((regs[PICA::InternalRegs::TexUnitCfg] & (1 << i)) == 0) {
			commandEncoder.setFragmentTexture(nullTexture, i);
			commandEncoder.setFragmentSamplerState(nearestSampler, i);
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
			auto tex = getTexture(targetTex);
			commandEncoder.setFragmentTexture(tex.texture, i);
			commandEncoder.setFragmentSamplerState(tex.sampler ? tex.sampler : nearestSampler, i);
		} else {
			// TODO: Bind a blank texture here. Some games, like Pokemon X, will render with a texture bound to nullptr, triggering GPU open bus
			// Binding a blank texture makes all of those games look normal
		}
	}
}

void RendererMTL::updateLightingLUT(MTL::RenderCommandEncoder* encoder) {
	gpu.lightingLUTDirty = false;

	std::array<u16, GPU::LightingLutSize> lightingLut;

	for (int i = 0; i < gpu.lightingLUT.size(); i++) {
		uint64_t value = gpu.lightingLUT[i] & 0xFFF;
		lightingLut[i] = (value << 4);
	}

	u32 index = lutLightingTexture->getNextIndex();
	lutLightingTexture->getTexture()->replaceRegion(
		MTL::Region(0, 0, LIGHTING_LUT_TEXTURE_WIDTH, Lights::LUT_Count), 0, index, lightingLut.data(), LIGHTING_LUT_TEXTURE_WIDTH * 2, 0
	);
}

void RendererMTL::updateFogLUT(MTL::RenderCommandEncoder* encoder) {
	gpu.fogLUTDirty = false;

	std::array<float, FOG_LUT_TEXTURE_WIDTH* 2> fogLut = {0.0f};

	for (int i = 0; i < fogLut.size(); i += 2) {
		const uint32_t value = gpu.fogLUT[i >> 1];
		int32_t diff = value & 0x1fff;
		diff = (diff << 19) >> 19;  // Sign extend the 13-bit value to 32 bits
		const float fogDifference = float(diff) / 2048.0f;
		const float fogValue = float((value >> 13) & 0x7ff) / 2048.0f;

		fogLut[i] = fogValue;
		fogLut[i + 1] = fogDifference;
	}

	u32 index = lutFogTexture->getNextIndex();
	lutFogTexture->getTexture()->replaceRegion(MTL::Region(0, 0, FOG_LUT_TEXTURE_WIDTH, 1), 0, index, fogLut.data(), 0, 0);
}

void RendererMTL::textureCopyImpl(
	Metal::ColorRenderTarget& srcFramebuffer, Metal::ColorRenderTarget& destFramebuffer, const Math::Rect<u32>& srcRect,
	const Math::Rect<u32>& destRect
) {
	nextRenderPassName = "Texture copy";
	MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
	// TODO: clearColor sets the load action to load if it didn't find any clear, but that is unnecessary if we are doing a copy to the whole
	// texture
	bool doesClear = clearColor(renderPassDescriptor, destFramebuffer.texture);
	beginRenderPassIfNeeded(renderPassDescriptor, doesClear, destFramebuffer.texture);

	// Pipeline
	Metal::BlitPipelineHash hash{destFramebuffer.format, DepthFmt::Unknown1};
	auto blitPipeline = blitPipelineCache.get(hash);

	commandEncoder.setRenderPipelineState(blitPipeline);

	// Viewport
	renderCommandEncoder->setViewport(MTL::Viewport{
		double(destRect.left), double(destRect.bottom), double(destRect.right - destRect.left), double(destRect.top - destRect.bottom), 0.0, 1.0});

	float srcRectNDC[4] = {
		srcRect.left / (float)srcFramebuffer.size.u(),
		srcRect.bottom / (float)srcFramebuffer.size.v(),
		(srcRect.right - srcRect.left) / (float)srcFramebuffer.size.u(),
		(srcRect.top - srcRect.bottom) / (float)srcFramebuffer.size.v(),
	};

	// Bind resources
	renderCommandEncoder->setVertexBytes(&srcRectNDC, sizeof(srcRectNDC), 0);
	renderCommandEncoder->setFragmentTexture(srcFramebuffer.texture, GET_HELPER_TEXTURE_BINDING(0));
	renderCommandEncoder->setFragmentSamplerState(nearestSampler, GET_HELPER_SAMPLER_STATE_BINDING(0));

	renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, NS::UInteger(0), NS::UInteger(4));
}

void RendererMTL::beginRenderPassIfNeeded(
	MTL::RenderPassDescriptor* renderPassDescriptor, bool doesClears, MTL::Texture* colorTexture, MTL::Texture* depthTexture
) {
	createCommandBufferIfNeeded();

	if (doesClears || !renderCommandEncoder || colorTexture != lastColorTexture ||
		(depthTexture != lastDepthTexture && !(lastDepthTexture && !depthTexture))) {
		endRenderPass();

		renderCommandEncoder = commandBuffer->renderCommandEncoder(renderPassDescriptor);
		renderCommandEncoder->setLabel(toNSString(nextRenderPassName));
		commandEncoder.newRenderCommandEncoder(renderCommandEncoder);

		// Bind persistent resources

		// LUT texture
		renderCommandEncoder->setFragmentTexture(lutLightingTexture->getTexture(), 3);
		renderCommandEncoder->setFragmentTexture(lutFogTexture->getTexture(), 4);
		renderCommandEncoder->setFragmentSamplerState(linearSampler, 3);

		lastColorTexture = colorTexture;
		lastDepthTexture = depthTexture;
	}

	renderPassDescriptor->release();
}
