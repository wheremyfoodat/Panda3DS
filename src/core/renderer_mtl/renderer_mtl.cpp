#include "renderer_mtl/renderer_mtl.hpp"

#include <cmrc/cmrc.hpp>
#include <cstddef>

#include "SDL_metal.h"

using namespace PICA;

CMRC_DECLARE(RendererMTL);

// Bind the vertex buffer to binding 30 so that it doesn't occupy the lower indices
#define VERTEX_BUFFER_BINDING_INDEX 30

RendererMTL::RendererMTL(GPU& gpu, const std::array<u32, regNum>& internalRegs, const std::array<u32, extRegNum>& externalRegs)
	: Renderer(gpu, internalRegs, externalRegs) {}
RendererMTL::~RendererMTL() {}

void RendererMTL::reset() {
	// TODO: implement
}

void RendererMTL::display() {
	createCommandBufferIfNeeded();

	CA::MetalDrawable* drawable = metalLayer->nextDrawable();

	MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
	MTL::RenderPassColorAttachmentDescriptor* colorAttachment = renderPassDescriptor->colorAttachments()->object(0);
	colorAttachment->setTexture(drawable->texture());
	colorAttachment->setLoadAction(MTL::LoadActionDontCare);
	colorAttachment->setStoreAction(MTL::StoreActionStore);

	MTL::RenderCommandEncoder* renderCommandEncoder = commandBuffer->renderCommandEncoder(renderPassDescriptor);
	renderCommandEncoder->setRenderPipelineState(displayPipeline);
	renderCommandEncoder->setFragmentTexture(topScreenTexture, 0);
	renderCommandEncoder->setFragmentSamplerState(basicSampler, 0);
	renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, NS::UInteger(0), NS::UInteger(4));

	renderCommandEncoder->endEncoding();

	commandBuffer->presentDrawable(drawable);
	commandBuffer->commit();
	commandBuffer = nullptr;
}

void RendererMTL::initGraphicsContext(SDL_Window* window) {
	// TODO: what should be the type of the view?
	void* view = SDL_Metal_CreateView(window);
	metalLayer = (CA::MetalLayer*)SDL_Metal_GetLayer(view);
	device = MTL::CreateSystemDefaultDevice();
	metalLayer->setDevice(device);
	commandQueue = device->newCommandQueue();

	// HACK
	MTL::TextureDescriptor* descriptor = MTL::TextureDescriptor::alloc()->init();
	descriptor->setTextureType(MTL::TextureType2D);
	descriptor->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
	descriptor->setWidth(400);
	descriptor->setHeight(240);
	descriptor->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
	topScreenTexture = device->newTexture(descriptor);

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
	// HACK
	auto* displayColorAttachment = displayPipelineDescriptor->colorAttachments()->object(0);
	displayColorAttachment->setPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);

	error = nullptr;
	displayPipeline = device->newRenderPipelineState(displayPipelineDescriptor, &error);
	if (error) {
		Helpers::panic("Error creating display pipeline state: %s", error->description()->cString(NS::ASCIIStringEncoding));
	}

	// Draw
	MTL::Function* vertexDrawFunction = library->newFunction(NS::String::string("vertexDraw", NS::ASCIIStringEncoding));
	MTL::Function* fragmentDrawFunction = library->newFunction(NS::String::string("fragmentDraw", NS::ASCIIStringEncoding));

	MTL::RenderPipelineDescriptor* drawPipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
	drawPipelineDescriptor->setVertexFunction(vertexDrawFunction);
	drawPipelineDescriptor->setFragmentFunction(fragmentDrawFunction);
	// HACK
	auto* drawColorAttachment = drawPipelineDescriptor->colorAttachments()->object(0);
	drawColorAttachment->setPixelFormat(topScreenTexture->pixelFormat());

	// Vertex descriptor
	// TODO: add all attributes
	MTL::VertexDescriptor* vertexDescriptor = MTL::VertexDescriptor::alloc()->init();
	MTL::VertexAttributeDescriptor* positionAttribute = vertexDescriptor->attributes()->object(0);
	positionAttribute->setFormat(MTL::VertexFormatFloat4);
	positionAttribute->setOffset(offsetof(Vertex, s.positions));
	positionAttribute->setBufferIndex(VERTEX_BUFFER_BINDING_INDEX);
	MTL::VertexAttributeDescriptor* colorAttribute = vertexDescriptor->attributes()->object(2);
	colorAttribute->setFormat(MTL::VertexFormatFloat4);
	colorAttribute->setOffset(offsetof(Vertex, s.colour));
	colorAttribute->setBufferIndex(VERTEX_BUFFER_BINDING_INDEX);
	MTL::VertexBufferLayoutDescriptor* vertexBufferLayout = vertexDescriptor->layouts()->object(VERTEX_BUFFER_BINDING_INDEX);
	vertexBufferLayout->setStride(sizeof(Vertex));
	vertexBufferLayout->setStepFunction(MTL::VertexStepFunctionPerVertex);
	vertexBufferLayout->setStepRate(1);
	drawPipelineDescriptor->setVertexDescriptor(vertexDescriptor);

	error = nullptr;
	drawPipeline = device->newRenderPipelineState(drawPipelineDescriptor, &error);
	if (error) {
		Helpers::panic("Error creating draw pipeline state: %s", error->description()->cString(NS::ASCIIStringEncoding));
	}
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
	createCommandBufferIfNeeded();

	std::cout << "DRAWING " << vertices.size() << " VERTICES" << std::endl;

	// TODO: don't begin a new render pass every time
	MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
	MTL::RenderPassColorAttachmentDescriptor* colorAttachment = renderPassDescriptor->colorAttachments()->object(0);
	colorAttachment->setTexture(topScreenTexture);
	colorAttachment->setLoadAction(MTL::LoadActionLoad);
	colorAttachment->setStoreAction(MTL::StoreActionStore);

	MTL::RenderCommandEncoder* renderCommandEncoder = commandBuffer->renderCommandEncoder(renderPassDescriptor);
	renderCommandEncoder->setRenderPipelineState(drawPipeline);
	renderCommandEncoder->setVertexBytes(vertices.data(), vertices.size_bytes(), VERTEX_BUFFER_BINDING_INDEX);

	// TODO: respect primitive type
	renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(vertices.size()));

	renderCommandEncoder->endEncoding();
}

void RendererMTL::screenshot(const std::string& name) {
	// TODO: implement
}

void RendererMTL::deinitGraphicsContext() {
	// TODO: implement
}
