#include "renderer_mtl/renderer_mtl.hpp"

#include <cmrc/cmrc.hpp>

#include "SDL_metal.h"

CMRC_DECLARE(RendererMTL);

RendererMTL::RendererMTL(GPU& gpu, const std::array<u32, regNum>& internalRegs, const std::array<u32, extRegNum>& externalRegs)
	: Renderer(gpu, internalRegs, externalRegs) {}
RendererMTL::~RendererMTL() {}

void RendererMTL::reset() {
	// TODO: implement
}

void RendererMTL::display() {
	CA::MetalDrawable* drawable = metalLayer->nextDrawable();

	MTL::CommandBuffer* commandBuffer = commandQueue->commandBuffer();

	MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::alloc()->init();
	MTL::RenderPassColorAttachmentDescriptor* colorAttachment = renderPassDescriptor->colorAttachments()->object(0);
	colorAttachment->setTexture(drawable->texture());
	colorAttachment->setLoadAction(MTL::LoadActionClear);
	colorAttachment->setClearColor(MTL::ClearColor{1.0f, 0.0f, 0.0f, 1.0f});
	colorAttachment->setStoreAction(MTL::StoreActionStore);

	MTL::RenderCommandEncoder* renderCommandEncoder = commandBuffer->renderCommandEncoder(renderPassDescriptor);
	renderCommandEncoder->endEncoding();

	commandBuffer->presentDrawable(drawable);
	commandBuffer->commit();
}

void RendererMTL::initGraphicsContext(SDL_Window* window) {
	// TODO: what should be the type of the view?
	void* view = SDL_Metal_CreateView(window);
	metalLayer = (CA::MetalLayer*)SDL_Metal_GetLayer(view);
	device = MTL::CreateSystemDefaultDevice();
	metalLayer->setDevice(device);
	commandQueue = device->newCommandQueue();

	// HACK
	MTL::TextureDescriptor* descriptor = MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatRGBA8Unorm, 400, 240, false);
	topScreenTexture = device->newTexture(descriptor);

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
	auto* colorAttachment = displayPipelineDescriptor->colorAttachments()->object(0);
	colorAttachment->setPixelFormat(topScreenTexture->pixelFormat());

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
	colorAttachment->setPixelFormat(MTL::PixelFormatRGBA8Unorm);

	// Vertex descriptor
	// TODO: add all attributes
	MTL::VertexDescriptor* vertexDescriptor = MTL::VertexDescriptor::alloc()->init();
	MTL::VertexAttributeDescriptor* positionAttribute = vertexDescriptor->attributes()->object(0);
	positionAttribute->setFormat(MTL::VertexFormatFloat4);
	positionAttribute->setOffset(0);
	positionAttribute->setBufferIndex(0);
	MTL::VertexAttributeDescriptor* colorAttribute = vertexDescriptor->attributes()->object(2);
	colorAttribute->setFormat(MTL::VertexFormatFloat4);
	colorAttribute->setOffset(16);
	colorAttribute->setBufferIndex(0);
	MTL::VertexBufferLayoutDescriptor* vertexBufferLayout = vertexDescriptor->layouts()->object(0);
	vertexBufferLayout->setStride(32);
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
	// TODO: implement
}

void RendererMTL::screenshot(const std::string& name) {
	// TODO: implement
}

void RendererMTL::deinitGraphicsContext() {
	// TODO: implement
}
