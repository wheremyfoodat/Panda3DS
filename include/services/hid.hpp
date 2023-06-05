#pragma once
#include <array>
#include <optional>
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

namespace HID::Keys {
	enum : u32 {
		A = 1 << 0,
		B = 1 << 1,
		Select = 1 << 2,
		Start = 1 << 3,
		Right = 1 << 4,
		Left = 1 << 5,
		Up = 1 << 6,
		Down = 1 << 7,
		R = 1 << 8,
		L = 1 << 9,
		X = 1 << 10,
		Y = 1 << 11,

		GPIO0Inv = 1 << 12,  // Inverted value of GPIO bit 0
		GPIO14Inv = 1 << 13, // Inverted value of GPIO bit 14

		CirclePadRight = 1 << 28, // X >= 41
		CirclePadLeft = 1 << 29,  // X <= -41
		CirclePadUp = 1 << 30,    // Y >= 41
		CirclePadDown = 1 << 31   // Y <= -41
	};
}

// Circular dependency because we need HID to spawn events
class Kernel;

class HIDService {
	Handle handle = KernelHandles::HID;
	Memory& mem;
	Kernel& kernel;
	u8* sharedMem = nullptr; // Pointer to HID shared memory

	bool accelerometerEnabled;
	bool eventsInitialized;
	bool gyroEnabled;

	std::array<std::optional<Handle>, 5> events;

	MAKE_LOG_FUNCTION(log, hidLogger)

	// Service commands
	void enableAccelerometer(u32 messagePointer);
	void enableGyroscopeLow(u32 messagePointer);
	void getGyroscopeLowCalibrateParam(u32 messagePointer);
	void getGyroscopeCoefficient(u32 messagePointer);
	void getIPCHandles(u32 messagePointer);

public:
	HIDService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);

	void pressKey(u32 key);
	void releaseKey(u32 key);
	void setCirclepadX(u16 x);
	void setCirclepadY(u16 y);
	void updateInputs();

	void setSharedMem(u8* ptr) {
		sharedMem = ptr;
		if (ptr != nullptr) { // Zero-fill shared memory in case the process tries to read stale service data or vice versa
			std::memset(ptr, 0, 0x2b0);
		}
	}
};