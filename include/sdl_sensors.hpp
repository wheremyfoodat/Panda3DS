#pragma once

#include <glm/glm.hpp>
#include <numbers>

#include "services/hid.hpp"

namespace Sensors::SDL {
	// Convert the rotation data we get from SDL sensor events to rotation data we can feed right to HID
	// Returns [pitch, roll, yaw]
	static glm::vec3 convertRotation(glm::vec3 rotation) {
		// Convert the rotation from rad/s to deg/s and scale by the gyroscope coefficient in HID
		constexpr float scale = 180.f / std::numbers::pi * HIDService::gyroscopeCoeff;
		// The axes are also inverted, so invert scale before the multiplication.
		return rotation * -scale;
	}

	static glm::vec3 convertAcceleration(float* data) {
		// Set our cap to ~9 m/s^2. The 3DS sensors cap at -930 and +930, so values above this value will get clamped to 930
		// At rest (3DS laid flat on table), hardware reads around ~0 for x and z axis, and around ~480 for y axis due to gravity.
		// This code tries to mimic this approximately, with offsets based on measurements from my DualShock 4.
		static constexpr float accelMax = 9.f;

		s16 x = std::clamp<s16>(s16(data[0] / accelMax * 930.f), -930, +930);
		s16 y = std::clamp<s16>(s16(data[1] / (SDL_STANDARD_GRAVITY * accelMax) * 930.f - 350.f), -930, +930);
		s16 z = std::clamp<s16>(s16((data[2] - 2.1f) / accelMax * 930.f), -930, +930);

		return glm::vec3(x, y, z);
	}
}  // namespace Gyro::SDL