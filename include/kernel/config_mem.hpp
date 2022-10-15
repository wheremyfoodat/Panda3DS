#pragma once
#include "helpers.hpp"

// Configuration memory addresses
namespace ConfigMem {
	enum : u32 {
		KernelVersionMinor = 0x1FF80002,
		KernelVersionMajor = 0x1FF80003,
		EnvInfo = 0x1FF80014,
		AppMemAlloc = 0x1FF80040,
		Datetime0 = 0x1FF81020,
		LedState3D = 0x1FF81084
	};
}