#pragma once
#include "PICA/float_types.hpp"
#include <array>

// A representation of the output vertex as it comes out of the vertex shader, with padding and all
struct PicaVertex {
	using vec2f = std::array<Floats::f24, 2>;
	using vec3f = std::array<Floats::f24, 3>;
	using vec4f = std::array<Floats::f24, 4>;

	union {
		struct {
			vec4f positions;   // Vertex position
			vec4f quaternion;  // Quaternion specifying the normal/tangent frame (for fragment lighting)
			vec4f colour;      // Vertex color
			vec2f texcoord0;   // Texcoords for texture unit 0 (Only U and V, W is stored separately for 3D textures!)
			vec2f texcoord1;   // Texcoords for TU 1
			Floats::f24 texcoord0_w;   // W component for texcoord 0 if using a 3D texture
			u32 padding;       // Unused

			vec3f view;       // View vector (for fragment lighting)
			u32 padding2;     // Unused
			vec2f texcoord2;  // Texcoords for TU 2
		} s;

		// The software, non-accelerated vertex loader writes here and then reads specific components from the above struct
		Floats::f24 raw[0x20];
	};
	PicaVertex() {}
};
//Float is used here instead of Floats::f24 to ensure that Floats::f24 is properly sized for direct interpretations as a float by the render backend
#define ASSERT_POS(member, pos) static_assert(offsetof(PicaVertex, s.member) == pos * sizeof(float), "PicaVertex struct is broken!");

ASSERT_POS(positions, 0)
ASSERT_POS(quaternion, 4)
ASSERT_POS(colour, 8)
ASSERT_POS(texcoord0, 12)
ASSERT_POS(texcoord1, 14)
ASSERT_POS(texcoord0_w, 16)
ASSERT_POS(view, 18)
ASSERT_POS(texcoord2, 22)
#undef ASSERT_POS