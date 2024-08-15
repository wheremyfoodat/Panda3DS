#pragma once

#include <glm/glm.hpp>
#include <numbers>

#include "services/hid.hpp"

namespace Gyro::SDL {
	// Convert the rotation data we get from SDL sensor events to rotation data we can feed right to HID
	// Returns [pitch, roll, yaw]
	static glm::vec3 convertRotation(glm::vec3 rotation) {
		// Convert the rotation from rad/s to deg/s and scale by the gyroscope coefficient in HID
		constexpr float scale = 180.f / std::numbers::pi * HIDService::gyroscopeCoeff;
		// The axes are also inverted, so invert scale before the multiplication.
		return rotation * -scale;
	}
}  // namespace Gyro::SDL