#pragma once

#include <glm/glm.hpp>
#include <numbers>

#include "services/hid.hpp"

namespace Gyro::SDL {
	// Convert the rotation data we get from SDL sensor events to rotation data we can feed right to HID
	// Returns [pitch, roll, yaw]
	static glm::vec3 convertRotation(glm::vec3 rotation) { 
		// Flip axes
		glm::vec3 ret = -rotation;
		// Convert from radians/s to deg/s and scale by the gyroscope coefficient from the HID service
		ret *= 180.f / std::numbers::pi;
		ret *= HIDService::gyroscopeCoeff;

		return ret;
	}
}  // namespace Gyro::SDL