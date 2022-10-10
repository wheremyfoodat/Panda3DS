#pragma once
#include "helpers.hpp"

using Handle = u32;

namespace KernelHandles {
	enum : u32 {
		Max = 0xFFFF7FFF, // Max handle the kernel can automagically allocate

		// Hardcoded handles
		CurrentThread = 0xFFFF8000,  // Used by the original kernel
		CurrentProcess = 0xFFFF8001, // Used by the original kernel
		APT = 0xFFFF8002,            // App Title something service?
		CFG = 0xFFFF8003,            // Console & region info
		HID = 0xFFFF8004,            // Handles everything input-related including gyro
		FS  = 0xFFFF8005,            // Filesystem service
		GPU = 0xFFFF8006,            // GPU service
		DSP = 0xFFFF8007,            // DSP service (Used for audio decoding and output)
		LCD = 0xFFFF8008,            // LCD service (Used for configuring the displays)
		NDM = 0xFFFF8009,            // ?????

		MinServiceHandle = APT,
		MaxServiceHandle = NDM,

		GSPSharedMemHandle = MaxServiceHandle + 1, // Handle for the GSP shared memory
		HIDSharedMemHandle,

		MinSharedMemHandle = GSPSharedMemHandle,
		MaxSharedMemHandle = HIDSharedMemHandle,

		HIDEvent0,
		HIDEvent1,
		HIDEvent2,
		HIDEvent3,
		HIDEvent4
	};

	// Returns whether "handle" belongs to one of the OS services
	static constexpr bool isServiceHandle(Handle handle) {
		return handle >= MinServiceHandle && handle <= MaxServiceHandle;
	}

	// Returns whether "handle" belongs to one of the OS services' shared memory areas
	static constexpr bool isSharedMemHandle(Handle handle) {
		return handle >= MinSharedMemHandle && handle <= MaxSharedMemHandle;
	}

	// Returns the name of a handle as a string based on the given handle
	static const char* getServiceName(Handle handle) {
		switch (handle) {
			case APT: return "APT";
			case CFG: return "CFG";
			case HID: return "HID";
			case FS: return "FS";
			case DSP: return "DSP";
			case GPU: return "GPU";
			case LCD: return "LCD";
			case NDM: return "NDM";
			default: return "Unknown";
		}
	}
}