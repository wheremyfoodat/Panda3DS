#include "PICA/gpu.hpp"

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdio>

#include "PICA/float_types.hpp"
#include "PICA/regs.hpp"
#include "renderer_null/renderer_null.hpp"
#include "renderer_sw/renderer_sw.hpp"
#ifdef PANDA3DS_ENABLE_OPENGL
#include "renderer_gl/renderer_gl.hpp"
#endif
#ifdef PANDA3DS_ENABLE_VULKAN
#include "renderer_vk/renderer_vk.hpp"
#endif

constexpr u32 topScreenWidth = 240;
constexpr u32 topScreenHeight = 400;

constexpr u32 bottomScreenWidth = 240;
constexpr u32 bottomScreenHeight = 300;

using namespace Floats;

// Note: For when we have multiple backends, the GL state manager can stay here and have the constructor for the Vulkan-or-whatever renderer ignore it
// Thus, our GLStateManager being here does not negatively impact renderer-agnosticness
GPU::GPU(Memory& mem, EmulatorConfig& config) : mem(mem), config(config) {
	vram = new u8[vramSize];
	mem.setVRAM(vram);  // Give the bus a pointer to our VRAM

	switch (config.rendererType) {
		case RendererType::Null: {
			renderer.reset(new RendererNull(*this, regs, externalRegs));
			break;
		}

		case RendererType::Software: {
			renderer.reset(new RendererSw(*this, regs, externalRegs));
			break;
		}

#ifdef PANDA3DS_ENABLE_OPENGL
		case RendererType::OpenGL: {
			renderer.reset(new RendererGL(*this, regs, externalRegs));
			break;
		}
#endif
#ifdef PANDA3DS_ENABLE_VULKAN
		case RendererType::Vulkan: {
			renderer.reset(new RendererVK(*this, regs, externalRegs));
			break;
		}
#endif
		default: {
			Helpers::panic("Rendering backend not supported: %s", Renderer::typeToString(config.rendererType));
			break;
		}
	}
}

void GPU::reset() {
	regs.fill(0);
	shaderUnit.reset();
	shaderJIT.reset();
	std::memset(vram, 0, vramSize);
	lightingLUT.fill(0);
	lightingLUTDirty = true;

	totalAttribCount = 0;
	fixedAttribMask = 0;
	fixedAttribIndex = 0;
	fixedAttribCount = 0;
	immediateModeAttrIndex = 0;
	immediateModeVertIndex = 0;

	fixedAttrBuff.fill(0);

	oldVsOutputMask = 0;
	setVsOutputMask(0xFFFF);

	for (auto& e : attributeInfo) {
		e.offset = 0;
		e.size = 0;
		e.config1 = 0;
		e.config2 = 0;
	}

	// Initialize the framebuffer registers. Values taken from Citra.

	using namespace PICA::ExternalRegs;
	// Top screen addresses and dimentions.
	externalRegs[Framebuffer0AFirstAddr] = 0x181E6000;
	externalRegs[Framebuffer0ASecondAddr] = 0x1822C800;
	externalRegs[Framebuffer0BFirstAddr] = 0x18273000;
	externalRegs[Framebuffer0BSecondAddr] = 0x182B9800;
	externalRegs[Framebuffer0Size] = (topScreenHeight << 16) | topScreenWidth;
	externalRegs[Framebuffer0Stride] = 720;
	externalRegs[Framebuffer0Config] = static_cast<u32>(PICA::ColorFmt::RGB8);
	externalRegs[Framebuffer0Select] = 0;

	// Bottom screen addresses and dimentions.
	externalRegs[Framebuffer1AFirstAddr] = 0x1848F000;
	externalRegs[Framebuffer1ASecondAddr] = 0x184C7800;
	externalRegs[Framebuffer1Size] = (bottomScreenHeight << 16) | bottomScreenWidth;
	externalRegs[Framebuffer1Stride] = 720;
	externalRegs[Framebuffer1Config] = static_cast<u32>(PICA::ColorFmt::RGB8);
	externalRegs[Framebuffer1Select] = 0;

	renderer->reset();
}

// Call the correct version of drawArrays based on whether this is an indexed draw (first template parameter)
// And whether we are going to use the shader JIT (second template parameter)
void GPU::drawArrays(bool indexed) {
	const bool shaderJITEnabled = ShaderJIT::isAvailable() && config.shaderJitEnabled;

	if (indexed) {
		if (shaderJITEnabled)
			drawArrays<true, true>();
		else
			drawArrays<true, false>();
	} else {
		if (shaderJITEnabled)
			drawArrays<false, true>();
		else
			drawArrays<false, false>();
	}
}

static std::array<PICA::Vertex, Renderer::vertexBufferSize> vertices;

template <bool indexed, bool useShaderJIT>
void GPU::drawArrays() {
	if constexpr (useShaderJIT) {
		shaderJIT.prepare(shaderUnit.vs);
	}

	setVsOutputMask(regs[PICA::InternalRegs::VertexShaderOutputMask]);

	// Base address for vertex attributes
	// The vertex base is always on a quadword boundary because the PICA does weird alignment shit any time possible
	const u32 vertexBase = ((regs[PICA::InternalRegs::VertexAttribLoc] >> 1) & 0xfffffff) * 16;
	const u32 vertexCount = regs[PICA::InternalRegs::VertexCountReg];  // Total # of vertices to transfer

	// Configures the type of primitive and the number of vertex shader outputs
	const u32 primConfig = regs[PICA::InternalRegs::PrimitiveConfig];
	const PICA::PrimType primType = static_cast<PICA::PrimType>(Helpers::getBits<8, 2>(primConfig));
	if (vertexCount > Renderer::vertexBufferSize) Helpers::panic("[PICA] vertexCount > vertexBufferSize");

	if ((primType == PICA::PrimType::TriangleList && vertexCount % 3) || (primType == PICA::PrimType::TriangleStrip && vertexCount < 3) ||
		(primType == PICA::PrimType::TriangleFan && vertexCount < 3)) {
		Helpers::panic("Invalid vertex count for primitive. Type: %d, vert count: %d\n", primType, vertexCount);
	}

	// Get the configuration for the index buffer, used only for indexed drawing
	u32 indexBufferConfig = regs[PICA::InternalRegs::IndexBufferConfig];
	u32 indexBufferPointer = vertexBase + (indexBufferConfig & 0xfffffff);
	bool shortIndex = Helpers::getBit<31>(indexBufferConfig);  // Indicates whether vert indices are 16-bit or 8-bit

	// Stuff the global attribute config registers in one u64 to make attr parsing easier
	// TODO: Cache this when the vertex attribute format registers are written to
	u64 vertexCfg = u64(regs[PICA::InternalRegs::AttribFormatLow]) | (u64(regs[PICA::InternalRegs::AttribFormatHigh]) << 32);

	if constexpr (!indexed) {
		u32 offset = regs[PICA::InternalRegs::VertexOffsetReg];
		log("PICA::DrawArrays(vertex count = %d, vertexOffset = %d)\n", vertexCount, offset);
	} else {
		log("PICA::DrawElements(vertex count = %d, index buffer config = %08X)\n", vertexCount, indexBufferConfig);
	}

	// Total number of input attributes to shader. Differs between GS and VS. Currently stubbed to the VS one, as we don't have geometry shaders.
	const u32 inputAttrCount = (regs[PICA::InternalRegs::VertexShaderInputBufferCfg] & 0xf) + 1;
	const u64 inputAttrCfg = getVertexShaderInputConfig();

	// When doing indexed rendering, we have a cache of vertices to avoid processing attributes and shaders for a single vertex many times
	constexpr bool vertexCacheEnabled = true;
	constexpr size_t vertexCacheSize = 64;

	struct {
		std::bitset<vertexCacheSize> validBits{0};         // Shows which tags are valid. If the corresponding bit is 1, then there's an entry
		std::array<u32, vertexCacheSize> ids;              // IDs (ie indices of the cached vertices in the 3DS vertex buffer)
		std::array<u32, vertexCacheSize> bufferPositions;  // Positions of the cached vertices in our own vertex buffer
	} vertexCache;

	for (u32 i = 0; i < vertexCount; i++) {
		u32 vertexIndex;  // Index of the vertex in the VBO for indexed rendering

		if constexpr (!indexed) {
			vertexIndex = i + regs[PICA::InternalRegs::VertexOffsetReg];
		} else {
			if (shortIndex) {
				auto ptr = getPointerPhys<u16>(indexBufferPointer);
				vertexIndex = *ptr;  // TODO: This is very unsafe
				indexBufferPointer += 2;
			} else {
				auto ptr = getPointerPhys<u8>(indexBufferPointer);
				vertexIndex = *ptr;  // TODO: This is also very unsafe
				indexBufferPointer += 1;
			}
		}

		// Check if the vertex corresponding to the index is in cache
		if constexpr (indexed && vertexCacheEnabled) {
			auto& cache = vertexCache;
			size_t tag = vertexIndex % vertexCacheSize;
			// Cache hit
			if (cache.validBits[tag] && cache.ids[tag] == vertexIndex) {
				vertices[i] = vertices[cache.bufferPositions[tag]];
				continue;
			}

			// Cache miss. Set cache entry, fetch attributes and run shaders as normal
			else {
				cache.validBits[tag] = true;
				cache.ids[tag] = vertexIndex;
				cache.bufferPositions[tag] = i;
			}
		}

		int attrCount = 0;
		int buffer = 0;  // Vertex buffer index for non-fixed attributes

		while (attrCount < totalAttribCount) {
			// Check if attribute is fixed or not
			if (fixedAttribMask & (1 << attrCount)) {                         // Fixed attribute
				vec4f& fixedAttr = shaderUnit.vs.fixedAttributes[attrCount];  // TODO: Is this how it works?
				vec4f& inputAttr = currentAttributes[attrCount];
				std::memcpy(&inputAttr, &fixedAttr, sizeof(vec4f));  // Copy fixed attr to input attr
				attrCount++;
			} else {                                 // Non-fixed attribute
				auto& attr = attributeInfo[buffer];  // Get information for this attribute
				u64 attrCfg = attr.getConfigFull();  // Get config1 | (config2 << 32)
				u32 attrAddress = vertexBase + attr.offset + (vertexIndex * attr.size);

				for (int j = 0; j < attr.componentCount; j++) {
					uint index = (attrCfg >> (j * 4)) & 0xf;  // Get index of attribute in vertexCfg

					// Vertex attributes used as padding
					// 12, 13, 14 and 15 are equivalent to 4, 8, 12 and 16 bytes of padding respectively
					if (index >= 12) [[unlikely]] {
						// Align attribute address up to a 4 byte boundary
						attrAddress = (attrAddress + 3) & -4;
						attrAddress += (index - 11) << 2;
						continue;
					}

					u32 attribInfo = (vertexCfg >> (index * 4)) & 0xf;
					u32 attribType = attribInfo & 0x3;  //  Type of attribute(sbyte/ubyte/short/float)
					u32 size = (attribInfo >> 2) + 1;   // Total number of components

					// printf("vertex_attribute_strides[%d] = %d\n", attrCount, attr.size);
					vec4f& attribute = currentAttributes[attrCount];
					uint component;  // Current component

					switch (attribType) {
						case 0: {  // Signed byte
							s8* ptr = getPointerPhys<s8>(attrAddress);
							for (component = 0; component < size; component++) {
								float val = static_cast<float>(*ptr++);
								attribute[component] = f24::fromFloat32(val);
							}
							attrAddress += size * sizeof(s8);
							break;
						}

						case 1: {  // Unsigned byte
							u8* ptr = getPointerPhys<u8>(attrAddress);
							for (component = 0; component < size; component++) {
								float val = static_cast<float>(*ptr++);
								attribute[component] = f24::fromFloat32(val);
							}
							attrAddress += size * sizeof(u8);
							break;
						}

						case 2: {  // Short
							s16* ptr = getPointerPhys<s16>(attrAddress);
							for (component = 0; component < size; component++) {
								float val = static_cast<float>(*ptr++);
								attribute[component] = f24::fromFloat32(val);
							}
							attrAddress += size * sizeof(s16);
							break;
						}

						case 3: {  // Float
							float* ptr = getPointerPhys<float>(attrAddress);
							for (component = 0; component < size; component++) {
								float val = *ptr++;
								attribute[component] = f24::fromFloat32(val);
							}
							attrAddress += size * sizeof(float);
							break;
						}

						default: Helpers::panic("[PICA] Unimplemented attribute type %d", attribType);
					}

					// Fill the remaining attribute lanes with default parameters (1.0 for alpha/w, 0.0) for everything else
					// Corgi does this although I'm not sure if it's actually needed for anything.
					// TODO: Find out
					while (component < 4) {
						attribute[component] = (component == 3) ? f24::fromFloat32(1.0) : f24::fromFloat32(0.0);
						component++;
					}

					attrCount++;
				}
				buffer++;
			}
		}

		// Before running the shader, the PICA maps the fetched attributes from the attribute registers to the shader input registers
		// Based on the SH_ATTRIBUTES_PERMUTATION registers.
		// Ie it might attribute #0 to v2, #1 to v7, etc
		for (int j = 0; j < totalAttribCount; j++) {
			const u32 mapping = (inputAttrCfg >> (j * 4)) & 0xf;
			std::memcpy(&shaderUnit.vs.inputs[mapping], &currentAttributes[j], sizeof(vec4f));
		}

		if constexpr (useShaderJIT) {
			shaderJIT.run(shaderUnit.vs);
		} else {
			shaderUnit.vs.run();
		}

		PICA::Vertex& out = vertices[i];
		// Map shader outputs to fixed function properties
		const u32 totalShaderOutputs = regs[PICA::InternalRegs::ShaderOutputCount] & 7;
		for (int i = 0; i < totalShaderOutputs; i++) {
			const u32 config = regs[PICA::InternalRegs::ShaderOutmap0 + i];

			for (int j = 0; j < 4; j++) {  // pls unroll
				const u32 mapping = (config >> (j * 8)) & 0x1F;
				out.raw[mapping] = vsOutputRegisters[i][j];
			}
		}
	}

	renderer->drawVertices(primType, std::span(vertices).first(vertexCount));
}

PICA::Vertex GPU::getImmediateModeVertex() {
	setVsOutputMask(regs[PICA::InternalRegs::VertexShaderOutputMask]);

	PICA::Vertex v;
	const int totalAttrCount = (regs[PICA::InternalRegs::VertexShaderAttrNum] & 0xf) + 1;

	// Copy immediate mode attributes to vertex shader unit
	for (int i = 0; i < totalAttrCount; i++) {
		shaderUnit.vs.inputs[i] = immediateModeAttributes[i];
	}

	// Run VS and return vertex data. TODO: Don't hardcode offsets for each attribute
	shaderUnit.vs.run();
	
	// Map shader outputs to fixed function properties
	const u32 totalShaderOutputs = regs[PICA::InternalRegs::ShaderOutputCount] & 7;
	for (int i = 0; i < totalShaderOutputs; i++) {
		const u32 config = regs[PICA::InternalRegs::ShaderOutmap0 + i];

		for (int j = 0; j < 4; j++) {  // pls unroll
			const u32 mapping = (config >> (j * 8)) & 0x1F;
			v.raw[mapping] = vsOutputRegisters[i][j];
		}
	}

	return v;
}

void GPU::fireDMA(u32 dest, u32 source, u32 size) {
	log("[GPU] DMA of %08X bytes from %08X to %08X\n", size, source, dest);
	constexpr u32 vramStart = VirtualAddrs::VramStart;
	constexpr u32 vramSize = VirtualAddrs::VramSize;

	const u32 fcramStart = mem.getLinearHeapVaddr();
	constexpr u32 fcramSize = VirtualAddrs::FcramTotalSize;

	// Shows whether this transfer is an FCRAM->VRAM transfer that's trivially optimizable
	bool cpuToVRAM = true;

	if (dest - vramStart >= vramSize || size > (vramSize - (dest - vramStart))) [[unlikely]] {
		cpuToVRAM = false;
		Helpers::panic("GPU DMA does not target VRAM");
	}

	if (source - fcramStart >= fcramSize || size > (fcramSize - (dest - fcramStart))) [[unlikely]] {
		cpuToVRAM = false;
		// Helpers::panic("GPU DMA does not have FCRAM as its source");
	}

	if (cpuToVRAM) [[likely]] {
		// Valid, optimized FCRAM->VRAM DMA. TODO: Is VRAM->VRAM DMA allowed?
		u8* fcram = mem.getFCRAM();
		std::memcpy(&vram[dest - vramStart], &fcram[source - fcramStart], size);
	} else {
		printf("Non-trivially optimizable GPU DMA. Falling back to byte-by-byte transfer\n");

		for (u32 i = 0; i < size; i++) {
			mem.write8(dest + i, mem.read8(source + i));
		}
	}
}
