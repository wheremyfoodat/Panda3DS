#include "services/ir/circlepad_pro.hpp"

#include <array>
#include <string>
#include <vector>

using namespace IR;

void CirclePadPro::connect() {}
void CirclePadPro::disconnect() {}

void CirclePadPro::receivePayload(Payload payload) {
	const u8 type = payload[0];

	switch (type) {
		case CPPRequestID::ConfigurePolling:
			// TODO
			break;

		case CPPRequestID::ReadCalibrationData: {
			static constexpr std::array<u8, 0x40> calibrationData = {
				0x00, 0x00, 0x08, 0x80, 0x85, 0xEB, 0x11, 0x3F, 0x85, 0xEB, 0x11, 0x3F, 0xFF, 0xFF, 0xFF, 0xF5, 0xFF, 0x00, 0x08, 0x80, 0x85, 0xEB,
				0x11, 0x3F, 0x85, 0xEB, 0x11, 0x3F, 0xFF, 0xFF, 0xFF, 0x65, 0xFF, 0x00, 0x08, 0x80, 0x85, 0xEB, 0x11, 0x3F, 0x85, 0xEB, 0x11, 0x3F,
				0xFF, 0xFF, 0xFF, 0x65, 0xFF, 0x00, 0x08, 0x80, 0x85, 0xEB, 0x11, 0x3F, 0x85, 0xEB, 0x11, 0x3F, 0xFF, 0xFF, 0xFF, 0x65,
			};

			struct ReadCalibrationDataRequest {
				u8 requestID;
				u8 expectedResponseTime;
				u16_le offset;
				u16_le size;
			};
			static_assert(sizeof(ReadCalibrationDataRequest) == 6, "ReadCalibrationDataRequest has wrong size");

			if (payload.size() != sizeof(ReadCalibrationDataRequest)) {
				Helpers::warn("IR::ReadCalibrationData: Wrong request size");
				return;
			}

			ReadCalibrationDataRequest request;
			std::memcpy(&request, payload.data(), sizeof(request));

			const u16 offset = request.offset & ~0xF;
			const u16 size = request.size & ~0xF;

			if (static_cast<std::size_t>(offset + size) > calibrationData.size()) {
				Helpers::warn("IR::ReadCalibrationData: Read beyond the end of calibration data!!");
				return;
			}

			std::vector<u8> response(5 + size);
			response[0] = static_cast<u8>(CPPResponseID::ReadCalibrationData);
			std::memcpy(&response[1], &request.offset, sizeof(request.offset));
			std::memcpy(&response[3], &request.size, sizeof(request.size));
			std::memcpy(&response[5], calibrationData.data() + offset, size);
			sendCallback(response);
			break;
		}
	}
}
