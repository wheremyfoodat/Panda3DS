#pragma once

#include <array>

#include "helpers.hpp"

namespace PICA {
	struct DrawAcceleration {
		static constexpr u32 maxAttribCount = 12;

		struct AttributeInfo {
			u8* data;
			u32 offset;

			u8 type;
			u8 componentCount;
			bool fixed;

			std::array<float, 4> fixedValue;  // For fixed attributes
		};

		u8* vertexBuffer;
		u8* indexBuffer;

		// Minimum and maximum index in the index buffer for a draw call
		u16 minimumIndex, maximumIndex;
		u32 totalAttribCount;
		u32 vertexDataSize;

		std::array<AttributeInfo, maxAttribCount> attributeInfo;

		bool canBeAccelerated;
		bool indexed;
	};
}  // namespace PICA