#pragma once

#include <vector>

#include "types.hpp"

namespace IVFC {
	struct IVFCLevel {
		u64 logicalOffset;
		u64 size;
		u64 blockSize;
	};

	struct IVFC {
		u64 masterHashSize;
		std::vector<IVFCLevel> levels;
	};

	size_t parseIVFC(uintptr_t ivfcStart, IVFC& ivfc);
}  // namespace IVFC
