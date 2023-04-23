#include "services/y2r.hpp"
#include "ipc.hpp"
#include "kernel.hpp"

namespace Y2RCommands {
	enum : u32 {
		SetInputFormat = 0x00010040,
		SetOutputFormat = 0x00030040,
		SetTransferEndInterrupt = 0x000D0040,
		GetTransferEndEvent = 0x000F0000,
		StopConversion = 0x00270000,
		IsBusyConversion = 0x00280000,
		PingProcess = 0x002A0000,
		DriverInitialize = 0x002B0000
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
	};
}

void Y2RService::reset() {
	transferEndInterruptEnabled = false;
	transferEndEvent = std::nullopt;

	inputFmt = InputFormat::YUV422_Individual8;
	outputFmt = OutputFormat::RGB32;
}

void Y2RService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case Y2RCommands::DriverInitialize: driverInitialize(messagePointer); break;
		case Y2RCommands::GetTransferEndEvent: getTransferEndEvent(messagePointer); break;
		case Y2RCommands::IsBusyConversion: isBusyConversion(messagePointer); break;
		case Y2RCommands::PingProcess: pingProcess(messagePointer); break;
		case Y2RCommands::SetInputFormat: setInputFormat(messagePointer); break;
		case Y2RCommands::SetOutputFormat: setOutputFormat(messagePointer); break;
		case Y2RCommands::SetTransferEndInterrupt: setTransferEndInterrupt(messagePointer); break;
		case Y2RCommands::StopConversion: stopConversion(messagePointer); break;
		default: Helpers::panic("Y2R service requested. Command: %08X\n", command);
	}
}

void Y2RService::pingProcess(u32 messagePointer) {
	log("Y2R::PingProcess\n");
	mem.write32(messagePointer, IPC::responseHeader(0x2A, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0); // Connected number
}

void Y2RService::driverInitialize(u32 messagePointer) {
	log("Y2R::DriverInitialize\n");
	mem.write32(messagePointer, IPC::responseHeader(0x2B, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::getTransferEndEvent(u32 messagePointer) {
	log("Y2R::GetTransferEndEvent\n");
	if (!transferEndEvent.has_value())
		transferEndEvent = kernel.makeEvent(ResetType::OneShot);

	mem.write32(messagePointer, IPC::responseHeader(0xF, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 12, transferEndEvent.value());
}

void Y2RService::setTransferEndInterrupt(u32 messagePointer) {
	const bool enable = mem.read32(messagePointer + 4) != 0;
	log("Y2R::SetTransferEndInterrupt (enabled: %s)\n", enable ? "yes" : "no");

	mem.write32(messagePointer, IPC::responseHeader(0xD, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
	transferEndInterruptEnabled = enable;
}

// We don't need to actually do anything for this.
// Cause it assumes that
// a) Y2R conversion works
// b) It isn't instant
void Y2RService::stopConversion(u32 messagePointer) {
	log("Y2R::StopConversion\n");

	mem.write32(messagePointer, IPC::responseHeader(0x27, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// See above. Our Y2R conversion (when implemented) will be instant because there's really no point trying to delay it
// This is a modern enough console for us to screw timings
void Y2RService::isBusyConversion(u32 messagePointer) {
	log("Y2R::IsBusyConversion\n");

	mem.write32(messagePointer, IPC::responseHeader(0x28, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, static_cast<u32>(BusyStatus::NotBusy));
}

void Y2RService::setInputFormat(u32 messagePointer) {
	const u32 format = mem.read32(messagePointer + 4);
	log("Y2R::SetInputFormat (format = %d)\n", format);

	if (format > 4) {
		Helpers::warn("Warning: Invalid input format for Y2R conversion\n");
	} else {
		inputFmt = static_cast<InputFormat>(format);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::setOutputFormat(u32 messagePointer) {
	const u32 format = mem.read32(messagePointer + 4);
	log("Y2R::SetOutputFormat (format = %d)\n", format);

	if (format > 3) {
		Helpers::warn("Warning: Invalid output format for Y2R conversion\n");
	} else {
		outputFmt = static_cast<OutputFormat>(format);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}