#include "services/cam.hpp"

#include <vector>

#include "ipc.hpp"
#include "kernel.hpp"

namespace CAMCommands {
	enum : u32 {
		StartCapture = 0x00010040,
		GetBufferErrorInterruptEvent = 0x00060040,
		SetReceiving = 0x00070102,
		DriverInitialize = 0x00390000,
		DriverFinalize = 0x003A0000,
		SetTransferLines = 0x00090100,
		GetMaxLines = 0x000A0080,
		SetTransferBytes = 0x000B0100,
		GetTransferBytes = 0x000C0040,
		GetMaxBytes = 0x000D0080,
		SetTrimming = 0x000E0080,
		SetTrimmingParamsCenter = 0x00120140,
		SetSize = 0x001F00C0,  // Set size has different headers between cam:u and New3DS QTM module
		SetFrameRate = 0x00200080,
		SetContrast = 0x00230080,
		GetSuitableY2rStandardCoefficient = 0x00360000,
	};
}

// Helper struct for working with camera ports
class PortSelect {
	u32 value;

  public:
	PortSelect(u32 val) : value(val) {}
	bool isValid() const { return value < 4; }

	bool isSinglePort() const {
		// 1 corresponds to the first camera port and 2 corresponds to the second port
		return value == 1 || value == 2;
	}

	bool isBothPorts() const {
		// 3 corresponds to both ports
		return value == 3;
	}

	// Returns the index of the camera port, assuming that it's only a single port
	int getSingleIndex() const {
		if (!isSinglePort()) [[unlikely]] {
			Helpers::panic("Camera: getSingleIndex called for port with invalid value");
		}

		return value - 1;
	}

	std::vector<int> getPortIndices() const {
		switch (value) {
			case 1: return {0};     // Only port 1
			case 2: return {1};     // Only port 2
			case 3: return {0, 1};  // Both port 1 and port 2
			default: return {};     // No ports or invalid ports
		}
	}
};

void CAMService::reset() {
	for (auto& port : ports) {
		port.reset();
	}
}

void CAMService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case CAMCommands::DriverInitialize: driverInitialize(messagePointer); break;
		case CAMCommands::DriverFinalize: driverFinalize(messagePointer); break;
		case CAMCommands::GetBufferErrorInterruptEvent: getBufferErrorInterruptEvent(messagePointer); break;
		case CAMCommands::GetMaxBytes: getMaxBytes(messagePointer); break;
		case CAMCommands::GetMaxLines: getMaxLines(messagePointer); break;
		case CAMCommands::GetSuitableY2rStandardCoefficient: getSuitableY2RCoefficients(messagePointer); break;
		case CAMCommands::GetTransferBytes: getTransferBytes(messagePointer); break;
		case CAMCommands::SetContrast: setContrast(messagePointer); break;
		case CAMCommands::SetFrameRate: setFrameRate(messagePointer); break;
		case CAMCommands::SetReceiving: setReceiving(messagePointer); break;
		case CAMCommands::SetSize: setSize(messagePointer); break;
		case CAMCommands::SetTransferLines: setTransferLines(messagePointer); break;
		case CAMCommands::SetTrimming: setTrimming(messagePointer); break;
		case CAMCommands::SetTrimmingParamsCenter: setTrimmingParamsCenter(messagePointer); break;
		case CAMCommands::StartCapture: startCapture(messagePointer); break;

		default:
			Helpers::warn("Unimplemented CAM service requested. Command: %08X\n", command);
			mem.write32(messagePointer + 4, Result::Success);
			break;
	}
}

void CAMService::driverInitialize(u32 messagePointer) {
	log("CAM::DriverInitialize\n");
	mem.write32(messagePointer, IPC::responseHeader(0x39, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void CAMService::driverFinalize(u32 messagePointer) {
	log("CAM::DriverFinalize\n");
	mem.write32(messagePointer, IPC::responseHeader(0x3A, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void CAMService::setContrast(u32 messagePointer) {
	const u32 cameraSelect = mem.read32(messagePointer + 4);
	const u32 contrast = mem.read32(messagePointer + 8);

	log("CAM::SetPhotoMode (camera select = %d, contrast = %d)\n", cameraSelect, contrast);

	mem.write32(messagePointer, IPC::responseHeader(0x23, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void CAMService::setTransferBytes(u32 messagePointer) {
	const u32 portIndex = mem.read8(messagePointer + 4);
	const u32 bytes = mem.read16(messagePointer + 8);
	// ...why do these parameters even exist?
	const u16 width = mem.read16(messagePointer + 12);
	const u16 height = mem.read16(messagePointer + 16);
	const PortSelect port(portIndex);

	if (port.isValid()) {
		for (int i : port.getPortIndices()) {
			ports[i].transferBytes = bytes;
		}
	} else {
		Helpers::warn("CAM::SetTransferBytes: Invalid port\n");
	}

	log("CAM::SetTransferBytes (port = %d, bytes = %d, width = %d, height = %d)\n", portIndex, bytes, width, height);

	mem.write32(messagePointer, IPC::responseHeader(0x9, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void CAMService::setTransferLines(u32 messagePointer) {
	const u32 portIndex = mem.read8(messagePointer + 4);
	const u16 lines = mem.read16(messagePointer + 8);
	const u16 width = mem.read16(messagePointer + 12);
	const u16 height = mem.read16(messagePointer + 16);
	const PortSelect port(portIndex);

	if (port.isValid()) {
		const u32 transferBytes = lines * width * 2;

		for (int i : port.getPortIndices()) {
			ports[i].transferBytes = transferBytes;
		}
	} else {
		Helpers::warn("CAM::SetTransferLines: Invalid port\n");
	}

	log("CAM::SetTransferLines (port = %d, lines = %d, width = %d, height = %d)\n", portIndex, lines, width, height);

	mem.write32(messagePointer, IPC::responseHeader(0x9, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void CAMService::setFrameRate(u32 messagePointer) {
	const u32 cameraSelect = mem.read32(messagePointer + 4);
	const u32 framerate = mem.read32(messagePointer + 8);

	log("CAM::SetPhotoMode (camera select = %d, framerate = %d)\n", cameraSelect, framerate);

	mem.write32(messagePointer, IPC::responseHeader(0x20, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void CAMService::setSize(u32 messagePointer) {
	const u32 cameraSelect = mem.read32(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	const u32 context = mem.read32(messagePointer + 12);

	log("CAM::SetSize (camera select = %d, size = %d, context = %d)\n", cameraSelect, size, context);

	mem.write32(messagePointer, IPC::responseHeader(0x1F, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void CAMService::setTrimming(u32 messagePointer) {
	const u32 port = mem.read8(messagePointer + 4);
	const bool trim = mem.read8(messagePointer + 8) != 0;

	log("CAM::SetTrimming (port = %d, trimming = %s)\n", port, trim ? "enabled" : "disabled");

	mem.write32(messagePointer, IPC::responseHeader(0x0E, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void CAMService::setTrimmingParamsCenter(u32 messagePointer) {
	const u32 port = mem.read8(messagePointer + 4);
	const s16 trimWidth = s16(mem.read16(messagePointer + 8));
	const s16 trimHeight = s16(mem.read16(messagePointer + 12));
	const s16 cameraWidth = s16(mem.read16(messagePointer + 16));
	const s16 cameraHeight = s16(mem.read16(messagePointer + 20));

	log("CAM::SetTrimmingParamsCenter (port = %d), trim size = (%d, %d), camera size = (%d, %d)\n", port, trimWidth, trimHeight, cameraWidth,
		cameraHeight);

	mem.write32(messagePointer, IPC::responseHeader(0x12, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// Algorithm taken from Citra
// https://github.com/citra-emu/citra/blob/master/src/core/hle/service/cam/cam.cpp#L465
void CAMService::getMaxLines(u32 messagePointer) {
	const u16 width = mem.read16(messagePointer + 4);
	const u16 height = mem.read16(messagePointer + 8);
	log("CAM::GetMaxLines (width = %d, height = %d)\n", width, height);

	constexpr u32 MIN_TRANSFER_UNIT = 256;
	constexpr u32 MAX_BUFFER_SIZE = 2560;
	if (width * height * 2 % MIN_TRANSFER_UNIT != 0) {
		Helpers::panic("CAM::GetMaxLines out of range");
	} else {
		u32 lines = MAX_BUFFER_SIZE / width;
		if (lines > height) {
			lines = height;
		}

		u32 result = Result::Success;
		while (height % lines != 0 || (lines * width * 2 % MIN_TRANSFER_UNIT != 0)) {
			--lines;
			if (lines == 0) {
				Helpers::panic("CAM::GetMaxLines out of range");
				break;
			}
		}

		mem.write32(messagePointer, IPC::responseHeader(0xA, 2, 0));
		mem.write32(messagePointer + 4, result);
		mem.write16(messagePointer + 8, lines);
	}
}

void CAMService::getMaxBytes(u32 messagePointer) {
	const u16 width = mem.read16(messagePointer + 4);
	const u16 height = mem.read16(messagePointer + 8);
	log("CAM::GetMaxBytes (width = %d, height = %d)\n", width, height);

	constexpr u32 MIN_TRANSFER_UNIT = 256;
	constexpr u32 MAX_BUFFER_SIZE = 2560;
	if (width * height * 2 % MIN_TRANSFER_UNIT != 0) {
		Helpers::panic("CAM::GetMaxLines out of range");
	} else {
		u32 bytes = MAX_BUFFER_SIZE;

		while (width * height * 2 % bytes != 0) {
			bytes -= MIN_TRANSFER_UNIT;
		}

		mem.write32(messagePointer, IPC::responseHeader(0xA, 2, 0));
		mem.write32(messagePointer + 4, Result::Success);
		mem.write32(messagePointer + 8, bytes);
	}
}

void CAMService::getSuitableY2RCoefficients(u32 messagePointer) {
	log("CAM::GetSuitableY2RCoefficients\n");
	mem.write32(messagePointer, IPC::responseHeader(0x36, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	// Y2R standard coefficient value
	mem.write32(messagePointer + 8, 0);
}

void CAMService::getTransferBytes(u32 messagePointer) {
	const u32 portIndex = mem.read8(messagePointer + 4);
	const PortSelect port(portIndex);
	log("CAM::GetTransferBytes (port = %d)\n", portIndex);

	mem.write32(messagePointer, IPC::responseHeader(0x0C, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);

	if (port.isSinglePort()) {
		mem.write32(messagePointer + 8, ports[port.getSingleIndex()].transferBytes);
	} else {
		// TODO: This should return the proper error code
		Helpers::warn("CAM::GetTransferBytes: Invalid port index");
		mem.write32(messagePointer + 8, 0);
	}
}

void CAMService::getBufferErrorInterruptEvent(u32 messagePointer) {
	const u32 portIndex = mem.read8(messagePointer + 4);
	const PortSelect port(portIndex);
	log("CAM::GetBufferErrorInterruptEvent (port = %d)\n", portIndex);

	mem.write32(messagePointer, IPC::responseHeader(0x6, 1, 2));

	if (port.isSinglePort()) {
		auto& event = ports[port.getSingleIndex()].bufferErrorInterruptEvent;
		if (!event.has_value()) {
			event = kernel.makeEvent(ResetType::OneShot);
		}

		mem.write32(messagePointer + 4, Result::Success);
		mem.write32(messagePointer + 8, 0);
		mem.write32(messagePointer + 12, event.value());
	} else {
		Helpers::panic("CAM::GetBufferErrorInterruptEvent: Invalid port");
	}
}

void CAMService::setReceiving(u32 messagePointer) {
	const u32 destination = mem.read32(messagePointer + 4);
	const u32 portIndex = mem.read8(messagePointer + 8);
	const u32 size = mem.read32(messagePointer + 12);
	const u16 transferUnit = mem.read16(messagePointer + 16);
	const Handle process = mem.read32(messagePointer + 24);

	const PortSelect port(portIndex);
	log("CAM::SetReceiving (port = %d)\n", portIndex);

	mem.write32(messagePointer, IPC::responseHeader(0x7, 1, 2));

	if (port.isSinglePort()) {
		auto& event = ports[port.getSingleIndex()].receiveEvent;
		if (!event.has_value()) {
			event = kernel.makeEvent(ResetType::OneShot);
		}

		mem.write32(messagePointer + 4, Result::Success);
		mem.write32(messagePointer + 8, 0);
		mem.write32(messagePointer + 12, event.value());
	} else {
		Helpers::panic("CAM::SetReceiving: Invalid port");
	}
}

void CAMService::startCapture(u32 messagePointer) {
	const u32 portIndex = mem.read8(messagePointer + 4);
	const PortSelect port(portIndex);
	log("CAM::StartCapture (port = %d)\n", portIndex);

	mem.write32(messagePointer, IPC::responseHeader(0x01, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);

	if (port.isValid()) {
		for (int i : port.getPortIndices()) {
			auto& event = ports[i].receiveEvent;

			// Until we properly implement cameras, immediately signal the receive event
			if (event.has_value()) {
				kernel.signalEvent(event.value());
			}
		}
	} else {
		Helpers::warn("CAM::StartCapture: Invalid port index");
	}
}
