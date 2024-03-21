#pragma once
#include <array>
#include <optional>

#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

namespace HID::Keys {
	enum : u32 {
		Null = 0,
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

		GPIO0Inv = 1 << 12,   // Inverted value of GPIO bit 0
		GPIO14Inv = 1 << 13,  // Inverted value of GPIO bit 14

		CirclePadRight = 1 << 28,  // X >= 41
		CirclePadLeft = 1 << 29,   // X <= -41
		CirclePadUp = 1 << 30,     // Y >= 41
		CirclePadDown = 1u << 31   // Y <= -41
	};
}

// Circular dependency because we need HID to spawn events
class Kernel;

class HIDService {
	Handle handle = KernelHandles::HID;
	Memory& mem;
	Kernel& kernel;
	u8* sharedMem = nullptr;  // Pointer to HID shared memory

	uint nextPadIndex;
	uint nextTouchscreenIndex;
	uint nextAccelerometerIndex;
	uint nextGyroIndex;

	u32 newButtons;  // The button state currently being edited
	u32 oldButtons;  // The previous pad state

	s16 circlePadX, circlePadY;      // Circlepad state
	s16 touchScreenX, touchScreenY;  // Touchscreen state
	s16 roll, pitch, yaw;            // Gyroscope state

	bool accelerometerEnabled;
	bool eventsInitialized;
	bool gyroEnabled;
	bool touchScreenPressed;

	std::array<std::optional<Handle>, 5> events;

	MAKE_LOG_FUNCTION(log, hidLogger)

	// Service commands
	void disableAccelerometer(u32 messagePointer);
	void disableGyroscopeLow(u32 messagePointer);
	void enableAccelerometer(u32 messagePointer);
	void enableGyroscopeLow(u32 messagePointer);
	void getGyroscopeLowCalibrateParam(u32 messagePointer);
	void getGyroscopeCoefficient(u32 messagePointer);
	void getIPCHandles(u32 messagePointer);
	void getSoundVolume(u32 messagePointer);

	// Don't call these prior to initializing shared mem pls
	template <typename T>
	T readSharedMem(size_t offset) {
		return *(T*)&sharedMem[offset];
	}

	template <typename T>
	void writeSharedMem(size_t offset, T value) {
		*(T*)&sharedMem[offset] = value;
	}

  public:
	HIDService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);

	void pressKey(u32 mask) { newButtons |= mask; }
	void releaseKey(u32 mask) { newButtons &= ~mask; }
	void setKey(u32 mask, bool pressed) { pressed ? pressKey(mask) : releaseKey(mask); }

	u32 getOldButtons() const { return oldButtons; }
	s16 getCirclepadX() const { return circlePadX; }
	s16 getCirclepadY() const { return circlePadY; }

	void setCirclepadX(s16 x) {
		circlePadX = x;

		// Turn bits 28 and 29 off in the new button state, which indicate whether the circlepad is steering left or right
		// Then, set them according to the new value of x
		newButtons &= ~0x3000'0000;
		if (x >= 41)  // Pressing right
			newButtons |= 1 << 28;
		else if (x <= -41)  // Pressing left
			newButtons |= 1 << 29;
	}

	void setCirclepadY(s16 y) {
		circlePadY = y;

		// Turn bits 30 and 31 off in the new button state, which indicate whether the circlepad is steering up or down
		// Then, set them according to the new value of y
		newButtons &= ~0xC000'0000;
		if (y >= 41)  // Pressing up
			newButtons |= 1 << 30;
		else if (y <= -41)  // Pressing down
			newButtons |= 1 << 31;
	}

	void setRoll(s16 value) { roll = value; }
	void setPitch(s16 value) { pitch = value; }
	void setYaw(s16 value) { yaw = value; }

	void updateInputs(u64 currentTimestamp);

	void setSharedMem(u8* ptr) {
		sharedMem = ptr;
		if (ptr != nullptr) {  // Zero-fill shared memory in case the process tries to read stale service data or vice versa
			std::memset(ptr, 0, 0x2b0);
		}
	}

	void setTouchScreenPress(u16 x, u16 y) {
		touchScreenX = x;
		touchScreenY = y;
		touchScreenPressed = true;
	}

	void releaseTouchScreen() {
		touchScreenPressed = false;
	}

	bool isTouchScreenPressed() { return touchScreenPressed; }
};
