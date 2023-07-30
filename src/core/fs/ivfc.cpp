#include "fs/ivfc.hpp"

namespace IVFC {
	size_t parseIVFC(uintptr_t ivfcStart, IVFC& ivfc) {
		uintptr_t ivfcPointer = ivfcStart;

		char* ivfcCharPtr = (char*)ivfcPointer;
		if (ivfcCharPtr[0] != 'I' || ivfcCharPtr[1] != 'V' || ivfcCharPtr[2] != 'F' || ivfcCharPtr[3] != 'C') {
			printf("Invalid header on IVFC\n");
			return 0;
		}
		ivfcPointer += 4;

		u32 magicIdentifier = *(u32*)ivfcPointer;
		ivfcPointer += 4;

		// RomFS IVFC uses 0x10000, DISA/DIFF IVFC uses 0x20000 here
		if (magicIdentifier != 0x10000 && magicIdentifier != 0x20000) {
			printf("Invalid IVFC magic identifier: %08X\n", magicIdentifier);
			return 0;
		}

		if (magicIdentifier == 0x10000) {
			ivfc.masterHashSize = *(u32*)ivfcPointer;
			ivfcPointer += 4;
			// RomFS IVFC uses 3 levels
			ivfc.levels.resize(3);
		} else {
			ivfc.masterHashSize = *(u64*)ivfcPointer;
			ivfcPointer += 8;
			// DISA/DIFF IVFC uses 4 levels
			ivfc.levels.resize(4);
		}

		for (size_t i = 0; i < ivfc.levels.size(); i++) {
			IVFCLevel level;

			level.logicalOffset = *(u64*)ivfcPointer;
			ivfcPointer += 8;

			level.size = *(u64*)ivfcPointer;
			ivfcPointer += 8;

			// This field is in log2
			level.blockSize = 1 << *(u32*)ivfcPointer;
			ivfcPointer += 4;

			// Skip 4 reserved bytes
			ivfcPointer += 4;

			ivfc.levels[i] = level;
		}

		u64 ivfcDescriptorSize = *(u64*)ivfcPointer;
		ivfcPointer += 8;

		uintptr_t ivfcActualSize = ivfcPointer - ivfcStart;

		// According to 3DBrew, this is usually the case but not guaranteed
		if (ivfcActualSize != ivfcDescriptorSize) {
			printf("IVFC descriptor size mismatch: %lx != %lx\n", ivfcActualSize, ivfcDescriptorSize);
		}

		if (magicIdentifier == 0x10000 && ivfcActualSize != 0x5C) {
			// This is always 0x5C bytes long
			printf("Invalid IVFC size: %08x\n", (u32)ivfcActualSize);
			return 0;
		} else if (magicIdentifier == 0x20000 && ivfcActualSize != 0x78) {
			// This is always 0x78 bytes long
			printf("Invalid IVFC size: %08x\n", (u32)ivfcActualSize);
			return 0;
		}

		return ivfcActualSize;
	}
}  // namespace IVFC