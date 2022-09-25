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
		HID = 0xFFFF8003,            // Handles everything input-related including gyro
		FS  = 0xFFFF8004,            // Filesystem service
		GPU = 0xFFFF8005,            // GPU service
		LCD = 0xFFFF8006,            // LCD service
		NDM = 0xFFFF8007,            // ?????

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
			case HID: return "HID";
			case FS: return "FS";
			case GPU: return "GPU";
			case LCD: return "LCD";
			case NDM: return "NDM";
			default: return "Unknown";
		}
	}
}