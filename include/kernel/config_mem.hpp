#pragma once
#include "helpers.hpp"

// Configuration memory addresses
namespace ConfigMem {
	enum : u32 {
		KernelVersionMinor = 0x1FF80002,
		KernelVersionMajor = 0x1FF80003,
		SyscoreVer = 0x1FF80010,
		EnvInfo = 0x1FF80014,
		AppMemAlloc = 0x1FF80040,
		HardwareType = 0x1FF81004,
		Datetime0 = 0x1FF81020,
		WifiMac = 0x1FF81060,
		NetworkState = 0x1FF81067,
		LedState3D = 0x1FF81084,
		BatteryState = 0x1FF81085,
		Unknown1086 = 0x1FF81086,
		HeadphonesConnectedMaybe = 0x1FF810C0  // TODO: What is actually stored here?
	};

	// Shows what type of hardware we're running on
	namespace HardwareCodes {
		enum : u8 { Product = 1, Devboard = 2, Debugger = 3, Capture = 4 };
	}
}  // namespace ConfigMem
