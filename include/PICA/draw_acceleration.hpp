#pragma once

#include <array>

#include "helpers.hpp"

namespace PICA {
	struct DrawAcceleration {
		u8* vertexBuffer;
		u8* indexBuffer;

		// Minimum and maximum index in the index buffer for a draw call
		u16 minimumIndex, maximumIndex;
		u32 vertexDataSize;

		bool canBeAccelerated;
		bool indexed;
	};
}  // namespace PICA