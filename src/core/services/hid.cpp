#include "services/hid.hpp"
#include "ipc.hpp"
#include "kernel.hpp"
#include <bit>

namespace HIDCommands {
	enum : u32 {
		GetIPCHandles = 0x000A0000,
		EnableAccelerometer = 0x00110000,
		EnableGyroscopeLow = 0x00130000,
		GetGyroscopeLowRawToDpsCoefficient = 0x00150000,
		GetGyroscopeLowCalibrateParam = 0x00160000
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
	eventsInitialized = false;
	gyroEnabled = false;

	// Deinitialize HID events
	for (auto& e : events) {
		e = std::nullopt;
	}
}

void HIDService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case HIDCommands::EnableAccelerometer: enableAccelerometer(messagePointer); break;
		case HIDCommands::EnableGyroscopeLow: enableGyroscopeLow(messagePointer); break;
		case HIDCommands::GetGyroscopeLowCalibrateParam: getGyroscopeLowCalibrateParam(messagePointer); break;
		case HIDCommands::GetGyroscopeLowRawToDpsCoefficient: getGyroscopeCoefficient(messagePointer); break;
		case HIDCommands::GetIPCHandles: getIPCHandles(messagePointer); break;
		default: Helpers::panic("HID service requested. Command: %08X\n", command);
	}
}

void HIDService::enableAccelerometer(u32 messagePointer) {
	log("HID::EnableAccelerometer\n");
	accelerometerEnabled = true;

	mem.write32(messagePointer, IPC::responseHeader(0x11, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void HIDService::enableGyroscopeLow(u32 messagePointer) {
	log("HID::EnableGyroscopeLow\n");
	gyroEnabled = true;

	mem.write32(messagePointer, IPC::responseHeader(0x13, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void HIDService::getGyroscopeLowCalibrateParam(u32 messagePointer) {
	log("HID::GetGyroscopeLowCalibrateParam\n");
	constexpr s16 unit = 6700; // Approximately from Citra which took it from hardware

	mem.write32(messagePointer, IPC::responseHeader(0x16, 6, 0));
	mem.write32(messagePointer + 4, Result::Success);
	// Fill calibration data (for x/y/z depending on i)
	for (int i = 0; i < 3; i++) {
		const u32 pointer = messagePointer + 8 + i * 3 * sizeof(u16); // Pointer to write the calibration info for the current coordinate

		mem.write16(pointer, 0); // Zero point
		mem.write16(pointer + 1 * sizeof(u16), unit); // Positive unit point
		mem.write16(pointer + 2 * sizeof(u16), -unit); // Negative unit point
	}
}

void HIDService::getGyroscopeCoefficient(u32 messagePointer) {
	log("HID::GetGyroscopeLowRawToDpsCoefficient\n");

	constexpr float gyroscopeCoeff = 14.375f; // Same as retail 3DS
	mem.write32(messagePointer, IPC::responseHeader(0x15, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, std::bit_cast<u32, float>(gyroscopeCoeff));
}

void HIDService::getIPCHandles(u32 messagePointer) {
	log("HID::GetIPCHandles\n");

	// Initialize HID events
	if (!eventsInitialized) {
		eventsInitialized = true;

		for (auto& e : events) {
			e = kernel.makeEvent(ResetType::OneShot);
		}
	}

	mem.write32(messagePointer, IPC::responseHeader(0xA, 1, 7));
	mem.write32(messagePointer + 4, Result::Success); // Result code
	mem.write32(messagePointer + 8, 0x14000000); // Translation descriptor
	mem.write32(messagePointer + 12, KernelHandles::HIDSharedMemHandle); // Shared memory handle

	// Write HID event handles
	for (int i = 0; i < events.size(); i++) {
		mem.write32(messagePointer + 16 + sizeof(Handle) * i, events[i].value());
	}
}

void HIDService::pressKey(u32 key) { sharedMem[0]++; *(u32*)&sharedMem[0x28] |= key; }
void HIDService::releaseKey(u32 key) { sharedMem[0]++; *(u32*)&sharedMem[0x28] &= ~key; }
void HIDService::setCirclepadX(u16 x) { sharedMem[0]++; *(u16*)&sharedMem[0x28 + 0xC] = x; }
void HIDService::setCirclepadY(u16 y) { sharedMem[0]++; *(u16*)&sharedMem[0x28 + 0xC + 2] = y; }

// TODO: We don't currently have inputs but we must at least try to signal the HID key input events now and then
void HIDService::updateInputs() {
	// For some reason, the original developers decided to signal the HID events each time the OS rescanned inputs
	// Rather than once every time the state of a key, or the accelerometer state, etc is updated
	// This means that the OS will signal the events even if literally nothing happened
	// Some games such as Majora's Mask rely on this behaviour.
	if (eventsInitialized) {
		for (auto& e : events) {
			kernel.signalEvent(e.value());
		}
	}
}