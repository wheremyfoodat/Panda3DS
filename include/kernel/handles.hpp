#pragma once
#include "helpers.hpp"

using Handle = u32;

namespace KernelHandles {
	enum : u32 {
		Max = 0xFFFF7FFF, // Max handle the kernel can automagically allocate

		// Hardcoded handles
		CurrentThread = 0xFFFF8000,  // Used by the original kernel
		CurrentProcess = 0xFFFF8001, // Used by the original kernel
		AC,   // Something network related
		ACT,  // Handles NNID accounts
		AM,   // Application manager
		APT,  // App Title something service?
		BOSS, // Streetpass stuff?
		CAM,  // Camera service
		CECD, // More Streetpass stuff?
		CFG,  // CFG service (Console & region info)
		HID,  // HID service (Handles everything input-related including gyro)
        FRD,  // Friend service (Miiverse friend service)
		FS,   // Filesystem service
		GPU,  // GPU service
		DSP,  // DSP service (Used for audio decoding and output)
		LCD,  // LCD service (Used for configuring the displays)
		LDR_RO, // Loader service. Used for loading CROs.
		MIC,  // MIC service (Controls the microphone)
		NFC,  // NFC (Duh), used for Amiibo
		NIM,  // Updates, DLC, etc
		NDM,  // ?????
		PTM,  // PTM service (Used for accessing various console info, such as battery, shell and pedometer state)
		Y2R,  // Also does camera stuff

		MinServiceHandle = AC,
		MaxServiceHandle = Y2R,

		GSPSharedMemHandle = MaxServiceHandle + 1, // Handle for the GSP shared memory
		FontSharedMemHandle,
		HIDSharedMemHandle,

		MinSharedMemHandle = GSPSharedMemHandle,
		MaxSharedMemHandle = HIDSharedMemHandle,
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
			case AC: return "AC";
			case ACT: return "ACT";
			case AM: return "AM";
			case APT: return "APT";
			case BOSS: return "BOSS";
			case CAM: return "CAM";
			case CECD: return "CECD";
			case CFG: return "CFG";
			case HID: return "HID";
			case FRD: return "FRD";
			case FS: return "FS";
			case DSP: return "DSP";
			case GPU: return "GSP::GPU";
			case LCD: return "GSP::LCD";
			case LDR_RO: return "LDR:RO";
			case MIC: return "MIC";
			case NDM: return "NDM";
			case NFC: return "NFC";
			case NIM: return "NIM";
			case PTM: return "PTM";
			case Y2R: return "Y2R";
			default: return "Unknown";
		}
	}
}