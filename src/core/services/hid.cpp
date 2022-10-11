#include "services/hid.hpp"

namespace HIDCommands {
	enum : u32 {
		GetIPCHandles = 0x000A0000,
		EnableAccelerometer = 0x00110000,
		EnableGyroscopeLow = 0x00130000
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
		Failure = 0xFFFFFFFF
	};
}

void HIDService::reset() {
	sharedMem = nullptr;
	accelerometerEnabled = false;
	gyroEnabled = false;
}

void HIDService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case HIDCommands::EnableAccelerometer: enableAccelerometer(messagePointer); break;
		case HIDCommands::EnableGyroscopeLow: enableGyroscopeLow(messagePointer); break;
		case HIDCommands::GetIPCHandles: getIPCHandles(messagePointer); break;
		default: Helpers::panic("HID service requested. Command: %08X\n", command);
	}
}

void HIDService::enableAccelerometer(u32 messagePointer) {
	log("HID::EnableAccelerometer\n");
	mem.write32(messagePointer + 4, Result::Success);
	accelerometerEnabled = true;
}

void HIDService::enableGyroscopeLow(u32 messagePointer) {
	log("HID::EnableGyroscopeLow\n");
	mem.write32(messagePointer + 4, Result::Success);
	gyroEnabled = true;
}

void HIDService::getIPCHandles(u32 messagePointer) {
	log("HID::GetIPCHandles\n");
	mem.write32(messagePointer + 4, Result::Success); // Result code
	mem.write32(messagePointer + 8, 0x14000000); // Translation descriptor
	mem.write32(messagePointer + 12, KernelHandles::HIDSharedMemHandle); // Shared memory handle

	// HID event handles
	mem.write32(messagePointer + 16, KernelHandles::HIDEvent0);
	mem.write32(messagePointer + 20, KernelHandles::HIDEvent1);
	mem.write32(messagePointer + 24, KernelHandles::HIDEvent2);
	mem.write32(messagePointer + 28, KernelHandles::HIDEvent3);
	mem.write32(messagePointer + 32, KernelHandles::HIDEvent4);
}