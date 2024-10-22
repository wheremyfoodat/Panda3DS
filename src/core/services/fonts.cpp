// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Adapted from https://github.com/PabloMK7/citra/blob/master/src/core/hle/service/apt/bcfnt/bcfnt.cpp

#include "services/fonts.hpp"

namespace HLE::Fonts {
	void relocateSharedFont(u8* sharedFont, u32 newAddress) {
		constexpr u32 sharedFontStartOffset = 0x80;
		const u8* cfntData = &sharedFont[sharedFontStartOffset];

		CFNT cfnt;
		std::memcpy(&cfnt, cfntData, sizeof(cfnt));

		u32 assumedCmapOffset = 0;
		u32 assumedCwdhOffset = 0;
		u32 assumedTglpOffset = 0;
		u32 firstCmapOffset = 0;
		u32 firstCwdhOffset = 0;
		u32 firstTglpOffset = 0;

		// First discover the location of sections so that the rebase offset can be auto-detected
		u32 currentOffset = sharedFontStartOffset + cfnt.headerSize;
		for (uint block = 0; block < cfnt.numBlocks; ++block) {
			const u8* data = &sharedFont[currentOffset];

			SectionHeader sectionHeader;
			std::memcpy(&sectionHeader, data, sizeof(sectionHeader));

			if (firstCmapOffset == 0 && std::memcmp(sectionHeader.magic, "CMAP", 4) == 0) {
				firstCmapOffset = currentOffset;
			} else if (firstCwdhOffset == 0 && std::memcmp(sectionHeader.magic, "CWDH", 4) == 0) {
				firstCwdhOffset = currentOffset;
			} else if (firstTglpOffset == 0 && std::memcmp(sectionHeader.magic, "TGLP", 4) == 0) {
				firstTglpOffset = currentOffset;
			} else if (std::memcmp(sectionHeader.magic, "FINF", 4) == 0) {
				Fonts::FINF finf;
				std::memcpy(&finf, data, sizeof(finf));

				assumedCmapOffset = finf.cmapOffset - sizeof(SectionHeader);
				assumedCwdhOffset = finf.cwdhOffset - sizeof(SectionHeader);
				assumedTglpOffset = finf.tglpOffset - sizeof(SectionHeader);
			}

			currentOffset += sectionHeader.sectionSize;
		}

		u32 previousBase = assumedCmapOffset - firstCmapOffset;
		if ((previousBase != assumedCwdhOffset - firstCwdhOffset) || (previousBase != assumedTglpOffset - firstTglpOffset)) {
			Helpers::warn("You shouldn't be seeing this. Shared Font file offsets might be borked?");
		}

		u32 offset = newAddress - previousBase;

		// Reset pointer back to start of sections and do the actual rebase
		currentOffset = sharedFontStartOffset + cfnt.headerSize;
		for (uint block = 0; block < cfnt.numBlocks; ++block) {
			u8* data = &sharedFont[currentOffset];

			SectionHeader sectionHeader;
			std::memcpy(&sectionHeader, data, sizeof(sectionHeader));

			if (std::memcmp(sectionHeader.magic, "FINF", 4) == 0) {
				Fonts::FINF finf;
				std::memcpy(&finf, data, sizeof(finf));

				// Relocate the offsets in the FINF section
				finf.cmapOffset += offset;
				finf.cwdhOffset += offset;
				finf.tglpOffset += offset;

				std::memcpy(data, &finf, sizeof(finf));
			} else if (std::memcmp(sectionHeader.magic, "CMAP", 4) == 0) {
				Fonts::CMAP cmap;
				std::memcpy(&cmap, data, sizeof(cmap));

				// Relocate the offsets in the CMAP section
				if (cmap.nextCmapOffset != 0) {
					cmap.nextCmapOffset += offset;
				}

				std::memcpy(data, &cmap, sizeof(cmap));
			} else if (std::memcmp(sectionHeader.magic, "CWDH", 4) == 0) {
				Fonts::CWDH cwdh;
				std::memcpy(&cwdh, data, sizeof(cwdh));

				// Relocate the offsets in the CWDH section
				if (cwdh.nextCwdhOffset != 0) {
					cwdh.nextCwdhOffset += offset;
				}

				std::memcpy(data, &cwdh, sizeof(cwdh));
			} else if (std::memcmp(sectionHeader.magic, "TGLP", 4) == 0) {
				Fonts::TGLP tglp;
				std::memcpy(&tglp, data, sizeof(tglp));

				// Relocate the offsets in the TGLP section
				tglp.sheetDataOffset += offset;
				std::memcpy(data, &tglp, sizeof(tglp));
			}

			currentOffset += sectionHeader.sectionSize;
		}
	}
}  // namespace HLE::Fonts