#pragma once

#include <array>

#include "helpers.hpp"

namespace PICA {
	struct DrawAcceleration {
		static constexpr u32 maxAttribCount = 16;
		static constexpr u32 maxLoaderCount = 12;

		struct AttributeInfo {
			u32 offset;
			u32 stride;

			u8 type;
			u8 componentCount;

			std::array<float, 4> fixedValue;  // For fixed attributes
		};

		struct Loader {
			// Data to upload for this loader
			u8* data;
			usize size;
		};

		u8* indexBuffer;

		// Minimum and maximum index in the index buffer for a draw call
		u16 minimumIndex, maximumIndex;
		u32 totalAttribCount;
		u32 totalLoaderCount;
		u32 enabledAttributeMask;
		u32 fixedAttributes;
		u32 vertexDataSize;

		std::array<AttributeInfo, maxAttribCount> attributeInfo;
		std::array<Loader, maxLoaderCount> loaders;

		bool canBeAccelerated;
		bool indexed;
		bool useShortIndices;
	};
}  // namespace PICA