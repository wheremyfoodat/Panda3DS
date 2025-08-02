#pragma once
#include "helpers.hpp"

struct Dsp1 {
	// All sizes are in bytes unless otherwise specified
	u8 signature[0x100];
	u8 magic[4];
	u32 size;
	u8 codeMemLayout;
	u8 dataMemLayout;
	u8 pad[3];
	u8 specialType;
	u8 segmentCount;
	u8 flags;
	u32 specialStart;
	u32 specialSize;
	u64 zeroBits;

	struct Segment {
		u32 offs;     // Offset of the segment data
		u32 dspAddr;  // Start of the segment in 16-bit units
		u32 size;
		u8 pad[3];
		u8 type;
		u8 hash[0x20];
	};

	Segment segments[10];
};
