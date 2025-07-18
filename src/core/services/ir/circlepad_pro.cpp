#include "services/ir/circlepad_pro.hpp"

#include <array>
#include <string>
#include <vector>

using namespace IR;

void CirclePadPro::connect() {}
void CirclePadPro::disconnect() { scheduler.removeEvent(Scheduler::EventType::UpdateIR); }

void CirclePadPro::receivePayload(Payload payload) {
	const u8 type = payload[0];

	switch (type) {
		case CPPRequestID::ConfigurePolling: {
			// Convert polling period from ms to ns for easier use with the scheduler
			const s64 periodNs = s64(payload[1]) * 1000ll;
			// Convert to cycles
			period = Scheduler::nsToCycles(periodNs);

			scheduler.removeEvent(Scheduler::EventType::UpdateIR);
			scheduler.addEvent(Scheduler::EventType::UpdateIR, scheduler.currentTimestamp + period);
			break;
		}

		case CPPRequestID::ReadCalibrationData: {
			// Data from https://github.com/azahar-emu/azahar/blob/f8b8b6e53cf518a53c0ae5f0201660c1250c0393/src/core/hle/service/ir/extra_hid.cpp#L73
			static constexpr std::array<u8, 0x40> calibrationData = {
				0x00, 0x00, 0x08, 0x80, 0x85, 0xEB, 0x11, 0x3F, 0x85, 0xEB, 0x11, 0x3F, 0xFF, 0xFF, 0xFF, 0xF5, 0xFF, 0x00, 0x08, 0x80, 0x85, 0xEB,
				0x11, 0x3F, 0x85, 0xEB, 0x11, 0x3F, 0xFF, 0xFF, 0xFF, 0x65, 0xFF, 0x00, 0x08, 0x80, 0x85, 0xEB, 0x11, 0x3F, 0x85, 0xEB, 0x11, 0x3F,
				0xFF, 0xFF, 0xFF, 0x65, 0xFF, 0x00, 0x08, 0x80, 0x85, 0xEB, 0x11, 0x3F, 0x85, 0xEB, 0x11, 0x3F, 0xFF, 0xFF, 0xFF, 0x65,
			};

			struct ReadCalibrationDataRequest {
				u8 requestID;  // Always CPPRequestID::ReadCalibrationData for this command
				u8 unknown;
				u16_le offset;  // Offset in calibration data to read from
				u16_le size;    // Bytes in calibration data to read
			};
			static_assert(sizeof(ReadCalibrationDataRequest) == 6, "ReadCalibrationDataRequest has wrong size");

			if (payload.size() != sizeof(ReadCalibrationDataRequest)) {
				Helpers::warn("IR::ReadCalibrationData: Wrong request size");
				return;
			}

			ReadCalibrationDataRequest request;
			std::memcpy(&request, payload.data(), sizeof(request));

			// Get the offset and size for the calibration data read. Bottom 4 bits are ignored, aligning reads to 16 bytes
			const auto offset = usize(request.offset & ~0xF);
			const auto size = usize(request.size & ~0xF);

			if (offset + size > calibrationData.size()) {
				Helpers::warn("IR::ReadCalibrationData: Read beyond the end of calibration data!!");
				return;
			}

			std::vector<u8> response(size + 5);
			response[0] = CPPResponseID::ReadCalibrationData;
			std::memcpy(&response[1], &request.offset, sizeof(request.offset));
			std::memcpy(&response[3], &request.size, sizeof(request.size));
			std::memcpy(&response[5], calibrationData.data() + offset, size);
			sendCallback(response);
			break;
		}
	}
}
