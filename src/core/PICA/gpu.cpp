#include "PICA/gpu.hpp"

#include <array>
#include <bitset>
#include <cstdio>
#include <cstddef>

#include "PICA/float_types.hpp"
#include "PICA/regs.hpp"

using namespace Floats;

// A representation of the output vertex as it comes out of the vertex shader, with padding and all
struct OutputVertex {
	using vec2f = OpenGL::Vector<f24, 2>;
	using vec3f = OpenGL::Vector<f24, 3>;
	using vec4f = OpenGL::Vector<f24, 4>;

	union {
		struct {
			vec4f positions;   // Vertex position
			vec4f quaternion;  // Quaternion specifying the normal/tangent frame (for fragment lighting)
			vec4f colour;      // Vertex color
			vec2f texcoord0;   // Texcoords for texture unit 0 (Only U and V, W is stored separately for 3D textures!)
			vec2f texcoord1;   // Texcoords for TU 1
			f24 texcoord0_w;   // W component for texcoord 0 if using a 3D texture
			u32 padding;       // Unused

			vec3f view;       // View vector (for fragment lighting)
			u32 padding2;     // Unused
			vec2f texcoord2;  // Texcoords for TU 2
		} s;

		// The software, non-accelerated vertex loader writes here and then reads specific components from the above struct
		f24 raw[0x20];
	};
	OutputVertex() {}
};
#define ASSERT_POS(member, pos) static_assert(offsetof(OutputVertex, s.member) == pos * sizeof(f24), "OutputVertex struct is broken!");

ASSERT_POS(positions, 0)
ASSERT_POS(quaternion, 4)
ASSERT_POS(colour, 8)
ASSERT_POS(texcoord0, 12)
ASSERT_POS(texcoord1, 14)
ASSERT_POS(texcoord0_w, 16)
ASSERT_POS(view, 18)
ASSERT_POS(texcoord2, 22)

GPU::GPU(Memory& mem) : mem(mem), renderer(*this, regs) {
	vram = new u8[vramSize];
	mem.setVRAM(vram); // Give the bus a pointer to our VRAM
}

void GPU::reset() {
	regs.fill(0);
	shaderUnit.reset();
	shaderJIT.reset();
	std::memset(vram, 0, vramSize);

	totalAttribCount = 0;
	fixedAttribMask = 0;
	fixedAttribIndex = 0;
	fixedAttribCount = 0;
	immediateModeAttrIndex = 0;
	immediateModeVertIndex = 0;

	fixedAttrBuff.fill(0);

	for (auto& e : attributeInfo) {
		e.offset = 0;
		e.size = 0;
		e.config1 = 0;
		e.config2 = 0;
	}

	renderer.reset();
}

// Call the correct version of drawArrays based on whether this is an indexed draw (first template parameter)
// And whether we are going to use the shader JIT (second template parameter)
void GPU::drawArrays(bool indexed) {
	constexpr bool shaderJITEnabled = false; // TODO: Make a configurable option

	if (indexed) {
		if constexpr (ShaderJIT::isAvailable() && shaderJITEnabled)
			drawArrays<true, true>();
		else
			drawArrays<true, false>();
	} else {
		if constexpr (ShaderJIT::isAvailable() && shaderJITEnabled)
			drawArrays<false, true>();
		else
			drawArrays<false, false>();
	}
}

static std::array<Vertex, Renderer::vertexBufferSize> vertices;

template <bool indexed, bool useShaderJIT>
void GPU::drawArrays() {
	if constexpr (useShaderJIT) {
		shaderJIT.prepare(shaderUnit.vs);
	}

	// Base address for vertex attributes
	// The vertex base is always on a quadword boundary because the PICA does weird alignment shit any time possible
	const u32 vertexBase = ((regs[PICA::InternalRegs::VertexAttribLoc] >> 1) & 0xfffffff) * 16;
	const u32 vertexCount = regs[PICA::InternalRegs::VertexCountReg]; // Total # of vertices to transfer

	// Configures the type of primitive and the number of vertex shader outputs
	const u32 primConfig = regs[PICA::InternalRegs::PrimitiveConfig];
	const PICA::PrimType primType = static_cast<PICA::PrimType>(Helpers::getBits<8, 2>(primConfig));
	if (primType == PICA::PrimType::TriangleFan) Helpers::panic("[PICA] Tried to draw unimplemented shape %d\n", primType);
	if (vertexCount > Renderer::vertexBufferSize) Helpers::panic("[PICA] vertexCount > vertexBufferSize");

	if ((primType == PICA::PrimType::TriangleList && vertexCount % 3) ||
		(primType == PICA::PrimType::TriangleStrip && vertexCount < 3)) {
		Helpers::panic("Invalid vertex count for primitive. Type: %d, vert count: %d\n", primType, vertexCount);
	}

	// Get the configuration for the index buffer, used only for indexed drawing
	u32 indexBufferConfig = regs[PICA::InternalRegs::IndexBufferConfig];
	u32 indexBufferPointer = vertexBase + (indexBufferConfig & 0xfffffff);
	bool shortIndex = Helpers::getBit<31>(indexBufferConfig); // Indicates whether vert indices are 16-bit or 8-bit

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
		std::bitset<vertexCacheSize> validBits{0};           // Shows which tags are valid. If the corresponding bit is 1, then there's an entry
		std::array<u32, vertexCacheSize> ids;                // IDs (ie indices of the cached vertices in the 3DS vertex buffer)
		std::array<u32, vertexCacheSize> bufferPositions;    // Positions of the cached vertices in our own vertex buffer
	} vertexCache;
		
	for (u32 i = 0; i < vertexCount; i++) {
		u32 vertexIndex; // Index of the vertex in the VBO for indexed rendering

		if constexpr (!indexed) {
			vertexIndex = i + regs[PICA::InternalRegs::VertexOffsetReg];
		} else {
			if (shortIndex) {
				auto ptr = getPointerPhys<u16>(indexBufferPointer);
				vertexIndex = *ptr; // TODO: This is very unsafe
				indexBufferPointer += 2;
			} else {
				auto ptr = getPointerPhys<u8>(indexBufferPointer);
				vertexIndex = *ptr; // TODO: This is also very unsafe
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
		int buffer = 0; // Vertex buffer index for non-fixed attributes

		while (attrCount < totalAttribCount) {
			// Check if attribute is fixed or not
			if (fixedAttribMask & (1 << attrCount)) { // Fixed attribute
				vec4f& fixedAttr = shaderUnit.vs.fixedAttributes[attrCount]; // TODO: Is this how it works?
				vec4f& inputAttr = currentAttributes[attrCount];
				std::memcpy(&inputAttr, &fixedAttr, sizeof(vec4f)); // Copy fixed attr to input attr
				attrCount++;
			} else { // Non-fixed attribute
				auto& attr = attributeInfo[buffer]; // Get information for this attribute
				u64 attrCfg = attr.getConfigFull(); // Get config1 | (config2 << 32)
				u32 attrAddress = vertexBase + attr.offset + (vertexIndex * attr.size);

				for (int j = 0; j < attr.componentCount; j++) {
					uint index = (attrCfg >> (j * 4)) & 0xf; // Get index of attribute in vertexCfg
					if (index >= 12) Helpers::panic("[PICA] Vertex attribute used as padding");

					u32 attribInfo = (vertexCfg >> (index * 4)) & 0xf;
					u32 attribType = attribInfo & 0x3; //  Type of attribute(sbyte/ubyte/short/float)
					u32 size = (attribInfo >> 2) + 1; // Total number of components

					//printf("vertex_attribute_strides[%d] = %d\n", attrCount, attr.size);
					vec4f& attribute = currentAttributes[attrCount];
					uint component; // Current component

					switch (attribType) {
						case 0: { // Signed byte
							s8* ptr = getPointerPhys<s8>(attrAddress);
							for (component = 0; component < size; component++) {
								float val = static_cast<float>(*ptr++);
								attribute[component] = f24::fromFloat32(val);
							}
							attrAddress += size * sizeof(s8);
							break;
						}

						case 1: { // Unsigned byte
							u8* ptr = getPointerPhys<u8>(attrAddress);
							for (component = 0; component < size; component++) {
								float val = static_cast<float>(*ptr++);
								attribute[component] = f24::fromFloat32(val);
							}
							attrAddress += size * sizeof(u8);
							break;
						}

						case 2: { // Short
							s16* ptr = getPointerPhys<s16>(attrAddress);
							for (component = 0; component < size; component++) {
								float val = static_cast<float>(*ptr++);
								attribute[component] = f24::fromFloat32(val);
							}
							attrAddress += size * sizeof(s16);
							break;
						}

						case 3: { // Float
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

		OutputVertex out;
		// Map shader outputs to fixed function properties
		const u32 totalShaderOutputs = regs[PICA::InternalRegs::ShaderOutputCount] & 7;
		for (int i = 0; i < totalShaderOutputs; i++) {
			const u32 config = regs[PICA::InternalRegs::ShaderOutmap0 + i];

			for (int j = 0; j < 4; j++) { // pls unroll
				const u32 mapping = (config >> (j * 8)) & 0x1F;
				out.raw[mapping] = shaderUnit.vs.outputs[i][j];
			}
		}

		std::memcpy(&vertices[i].position, &out.s.positions, sizeof(vec4f));
		std::memcpy(&vertices[i].colour, &out.s.colour, sizeof(vec4f));
		std::memcpy(&vertices[i].texcoord0, &out.s.texcoord0, 2 * sizeof(f24));
		std::memcpy(&vertices[i].texcoord1, &out.s.texcoord1, 2 * sizeof(f24));
		std::memcpy(&vertices[i].texcoord0_w, &out.s.texcoord0_w, sizeof(f24));
		std::memcpy(&vertices[i].texcoord2, &out.s.texcoord2, 2 * sizeof(f24));

		//printf("(x, y, z, w) = (%f, %f, %f, %f)\n", (double)vertices[i].position.x(), (double)vertices[i].position.y(), (double)vertices[i].position.z(), (double)vertices[i].position.w());
		//printf("(r, g, b, a) = (%f, %f, %f, %f)\n", (double)vertices[i].colour.r(), (double)vertices[i].colour.g(), (double)vertices[i].colour.b(), (double)vertices[i].colour.a());
		//printf("(u, v      ) = (%f, %f)\n", vertices[i].UVs.u(), vertices[i].UVs.v());
	}

	renderer.drawVertices(primType, std::span(vertices).first(vertexCount));
}

Vertex GPU::getImmediateModeVertex() {
	Vertex v;
	const int totalAttrCount = (regs[PICA::InternalRegs::VertexShaderAttrNum] & 0xf) + 1;

	// Copy immediate mode attributes to vertex shader unit
	for (int i = 0; i < totalAttrCount; i++) {
		shaderUnit.vs.inputs[i] = immediateModeAttributes[i];
	}

	// Run VS and return vertex data. TODO: Don't hardcode offsets for each attribute
	shaderUnit.vs.run();
	std::memcpy(&v.position, &shaderUnit.vs.outputs[0], sizeof(vec4f));
	std::memcpy(&v.colour, &shaderUnit.vs.outputs[1], sizeof(vec4f));
	std::memcpy(&v.texcoord0, &shaderUnit.vs.outputs[2], 2 * sizeof(f24));

	printf("(x, y, z, w) = (%f, %f, %f, %f)\n", (double)v.position.x(), (double)v.position.y(), (double)v.position.z(), (double)v.position.w());
	printf("(r, g, b, a) = (%f, %f, %f, %f)\n", (double)v.colour.r(), (double)v.colour.g(), (double)v.colour.b(), (double)v.colour.a());
	printf("(u, v      ) = (%f, %f)\n", v.texcoord0.u(), v.texcoord0.v());

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
