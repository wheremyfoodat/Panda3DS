// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Adapted from https://github.com/PabloMK7/citra/blob/master/src/core/hle/service/apt/bcfnt/bcfnt.h

#pragma once

#include <memory>

#include "helpers.hpp"
#include "swap.hpp"

namespace HLE::Fonts {
	struct CFNT {
		u8 magic[4];
		u16_le endianness;
		u16_le headerSize;
		u32_le version;
		u32_le fileSize;
		u32_le numBlocks;
	};

	struct SectionHeader {
		u8 magic[4];
		u32_le sectionSize;
	};

	struct FINF {
		u8 magic[4];
		u32_le sectionSize;
		u8 fontType;
		u8 lineFeed;
		u16_le alterCharIndex;
		u8 default_width[3];
		u8 encoding;
		u32_le tglpOffset;
		u32_le cwdhOffset;
		u32_le cmapOffset;
		u8 height;
		u8 width;
		u8 ascent;
		u8 reserved;
	};

	struct TGLP {
		u8 magic[4];
		u32_le sectionSize;
		u8 cellWidth;
		u8 cellHeight;
		u8 baselinePosition;
		u8 maxCharacterWidth;
		u32_le sheetSize;
		u16_le numSheets;
		u16_le sheetImageFormat;
		u16_le numColumns;
		u16_le numRows;
		u16_le sheetWidth;
		u16_le sheetHeight;
		u32_le sheetDataOffset;
	};

	struct CMAP {
		u8 magic[4];
		u32_le sectionSize;
		u16_le codeBegin;
		u16_le codeEnd;
		u16_le mappingMethod;
		u16_le reserved;
		u32_le nextCmapOffset;
	};

	struct CWDH {
		u8 magic[4];
		u32_le sectionSize;
		u16_le startIndex;
		u16_le endIndex;
		u32_le nextCwdhOffset;
	};

	// Relocates the internal addresses of the BCFNT Shared Font to the new base. The current base will
	// be auto-detected based on the file headers.
	void relocateSharedFont(u8* sharedFont, u32 newAddress);
}  // namespace HLE::Fonts