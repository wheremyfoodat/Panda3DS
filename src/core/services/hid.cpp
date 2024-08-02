#include "services/hid.hpp"

#include <map>
#include <unordered_map>

#include "ipc.hpp"
#include "kernel.hpp"

namespace HID::Keys {
	const char* keyToName(u32 key) {
		static std::map<u32, const char*> keyMap = {
			{A, "A"},
			{B, "B"},
			{Select, "Select"},
			{Start, "Start"},
			{Right, "Right"},
			{Left, "Left"},
			{Up, "Up"},
			{Down, "Down"},
			{R, "R"},
			{L, "L"},
			{X, "X"},
			{Y, "Y"},
			{CirclePadRight, "CirclePad Right"},
			{CirclePadLeft, "CirclePad Left"},
			{CirclePadUp, "CirclePad Up"},
			{CirclePadDown, "CirclePad Down"},
		};

		auto it = keyMap.find(key);
		return it != keyMap.end() ? it->second : "Unknown key";
	}

	u32 nameToKey(std::string name) {
		static std::unordered_map<std::string, u32> keyMap = {
			{"a", A},
			{"b", B},
			{"select", Select},
			{"start", Start},
			{"right", Right},
			{"left", Left},
			{"up", Up},
			{"down", Down},
			{"r", R},
			{"l", L},
			{"x", X},
			{"y", Y},
			{"circlepad right", CirclePadRight},
			{"circlepad left", CirclePadLeft},
			{"circlepad up", CirclePadUp},
			{"circlepad down", CirclePadDown},
		};

		std::transform(name.begin(), name.end(), name.begin(), ::tolower);

		auto it = keyMap.find(name);
		return it != keyMap.end() ? it->second : HID::Keys::Null;
	}
}  // namespace HID::Keys

namespace HIDCommands {
	enum : u32 {
		GetIPCHandles = 0x000A0000,
		EnableAccelerometer = 0x00110000,
		DisableAccelerometer = 0x00120000,
		EnableGyroscopeLow = 0x00130000,
		DisableGyroscopeLow = 0x00140000,
		GetGyroscopeLowRawToDpsCoefficient = 0x00150000,
		GetGyroscopeLowCalibrateParam = 0x00160000,
		GetSoundVolume = 0x00170000,
	};
}

void HIDService::reset() {
	sharedMem = nullptr;
	accelerometerEnabled = false;
	eventsInitialized = false;
	gyroEnabled = false;
	touchScreenPressed = false;

	// Deinitialize HID events
	for (auto& e : events) {
		e = std::nullopt;
	}

	// Reset indices for the various HID shared memory entries
	nextPadIndex = nextTouchscreenIndex = nextAccelerometerIndex = nextGyroIndex = 0;
	// Reset button states
	newButtons = oldButtons = 0;
	circlePadX = circlePadY = 0;
	touchScreenX = touchScreenY = 0;
	roll = pitch = yaw = 0;
}

void HIDService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case HIDCommands::DisableAccelerometer: disableAccelerometer(messagePointer); break;
		case HIDCommands::DisableGyroscopeLow: disableGyroscopeLow(messagePointer); break;
		case HIDCommands::EnableAccelerometer: enableAccelerometer(messagePointer); break;
		case HIDCommands::EnableGyroscopeLow: enableGyroscopeLow(messagePointer); break;
		case HIDCommands::GetGyroscopeLowCalibrateParam: getGyroscopeLowCalibrateParam(messagePointer); break;
		case HIDCommands::GetGyroscopeLowRawToDpsCoefficient: getGyroscopeCoefficient(messagePointer); break;
		case HIDCommands::GetIPCHandles: getIPCHandles(messagePointer); break;
		case HIDCommands::GetSoundVolume: getSoundVolume(messagePointer); break;
		default: Helpers::panic("HID service requested. Command: %08X\n", command);
	}
}

void HIDService::enableAccelerometer(u32 messagePointer) {
	log("HID::EnableAccelerometer\n");
	accelerometerEnabled = true;

	mem.write32(messagePointer, IPC::responseHeader(0x11, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void HIDService::disableAccelerometer(u32 messagePointer) {
	log("HID::DisableAccelerometer\n");
	accelerometerEnabled = false;

	mem.write32(messagePointer, IPC::responseHeader(0x12, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void HIDService::enableGyroscopeLow(u32 messagePointer) {
	log("HID::EnableGyroscopeLow\n");
	gyroEnabled = true;

	mem.write32(messagePointer, IPC::responseHeader(0x13, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void HIDService::disableGyroscopeLow(u32 messagePointer) {
	log("HID::DisableGyroscopeLow\n");
	gyroEnabled = false;

	mem.write32(messagePointer, IPC::responseHeader(0x14, 1, 0));
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
	mem.write32(messagePointer + 8, Helpers::bit_cast<u32, float>(gyroscopeCoeff));
}

// The volume here is in the range [0, 0x3F]
// It is read directly from I2C Device 3 register 0x09
// Since we currently do not have audio, set the volume a bit below max (0x30)
void HIDService::getSoundVolume(u32 messagePointer) {
	log("HID::GetSoundVolume\n");
	constexpr u8 volume = 0x30;

	mem.write32(messagePointer, IPC::responseHeader(0x17, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, volume);
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

void HIDService::updateInputs(u64 currentTick) {
	// Update shared memory if it has been initialized
	if (sharedMem) {
		// First, update the pad state
		if (nextPadIndex == 0) {
			writeSharedMem<u64>(0x8, readSharedMem<u64>(0x0)); // Copy previous tick count
			writeSharedMem<u64>(0x0, currentTick);             // Write new tick count
		}

		writeSharedMem<u32>(0x10, nextPadIndex); // Index last updated by the HID module
		writeSharedMem<u32>(0x1C, newButtons);   // Current PAD state
		writeSharedMem<s16>(0x20, circlePadX);   // Current circle pad state
		writeSharedMem<s16>(0x22, circlePadY);

		const size_t padEntryOffset = 0x28 + (nextPadIndex * 0x10); // Offset in the array of 8 pad entries
		nextPadIndex = (nextPadIndex + 1) % 8; // Move to next entry

		const u32 pressed = (newButtons ^ oldButtons) & newButtons; // Pressed buttons
		const u32 released = (newButtons ^ oldButtons) & oldButtons; // Released buttons
		oldButtons = newButtons;

		writeSharedMem<u32>(padEntryOffset, newButtons);
		writeSharedMem<u32>(padEntryOffset + 4, pressed);
		writeSharedMem<u32>(padEntryOffset + 8, released);
		writeSharedMem<s16>(padEntryOffset + 12, circlePadX);
		writeSharedMem<s16>(padEntryOffset + 14, circlePadY);

		// Next, update touchscreen state
		if (nextTouchscreenIndex == 0) {
			writeSharedMem<u64>(0xB0, readSharedMem<u64>(0xA8)); // Copy previous tick count
			writeSharedMem<u64>(0xA8, currentTick);             // Write new tick count
		}
		writeSharedMem<u32>(0xB8, nextTouchscreenIndex); // Index last updated by the HID module
		const size_t touchEntryOffset = 0xC8 + (nextTouchscreenIndex * 8); // Offset in the array of 8 touchscreen entries
		nextTouchscreenIndex = (nextTouchscreenIndex + 1) % 8; // Move to next entry

		writeSharedMem<u16>(touchEntryOffset, touchScreenX);
		writeSharedMem<u16>(touchEntryOffset + 2, touchScreenY);
		writeSharedMem<u8>(touchEntryOffset + 4, touchScreenPressed ? 1 : 0);

		// Next, update accelerometer state
		if (nextAccelerometerIndex == 0) {
			writeSharedMem<u64>(0x110, readSharedMem<u64>(0x108)); // Copy previous tick count
			writeSharedMem<u64>(0x108, currentTick);             // Write new tick count
		}
		writeSharedMem<u32>(0x118, nextAccelerometerIndex); // Index last updated by the HID module
		nextAccelerometerIndex = (nextAccelerometerIndex + 1) % 8; // Move to next entry

		// Next, update gyro state
		if (nextGyroIndex == 0) {
			writeSharedMem<u64>(0x160, readSharedMem<u64>(0x158)); // Copy previous tick count
			writeSharedMem<u64>(0x158, currentTick);             // Write new tick count
		}
		const size_t gyroEntryOffset = 0x178 + (nextGyroIndex * 6);  // Offset in the array of 8 touchscreen entries
		writeSharedMem<u16>(gyroEntryOffset, pitch);
		writeSharedMem<u16>(gyroEntryOffset + 2, yaw);
		writeSharedMem<u16>(gyroEntryOffset + 4, roll);

		// Since gyroscope euler angles are relative, we zero them out here and the frontend will update them again when we receive a new rotation
		roll = pitch = yaw = 0;

		writeSharedMem<u32>(0x168, nextGyroIndex); // Index last updated by the HID module
		nextGyroIndex = (nextGyroIndex + 1) % 32; // Move to next entry
	}

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