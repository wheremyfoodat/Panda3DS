#include "services/y2r.hpp"
#include "ipc.hpp"
#include "kernel.hpp"

namespace Y2RCommands {
	enum : u32 {
		SetInputFormat = 0x00010040,
		SetOutputFormat = 0x00030040,
		SetRotation = 0x00050040,
		SetBlockAlignment = 0x00070040,
		SetSpacialDithering = 0x00090040,
		SetTemporalDithering = 0x000B0040,
		SetTransferEndInterrupt = 0x000D0040,
		GetTransferEndEvent = 0x000F0000,
		SetSendingY = 0x00100102,
		SetSendingU = 0x00110102,
		SetSendingV = 0x00120102,
		SetReceiving = 0x00180102,
		SetInputLineWidth = 0x001A0040,
		SetInputLines = 0x001C0040,
		SetStandardCoeff = 0x00200040,
		SetAlpha = 0x00220040,
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

	alignment = BlockAlignment::Line;
	inputFmt = InputFormat::YUV422_Individual8;
	outputFmt = OutputFormat::RGB32;
	rotation = Rotation::None;

	spacialDithering = false;
	temporalDithering = false;

	alpha = 0xFFFF;
	inputLines = 69;
	inputLineWidth = 420;
}

void Y2RService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case Y2RCommands::DriverInitialize: driverInitialize(messagePointer); break;
		case Y2RCommands::GetTransferEndEvent: getTransferEndEvent(messagePointer); break;
		case Y2RCommands::IsBusyConversion: isBusyConversion(messagePointer); break;
		case Y2RCommands::PingProcess: pingProcess(messagePointer); break;
		case Y2RCommands::SetAlpha: setAlpha(messagePointer); break;
		case Y2RCommands::SetBlockAlignment: setBlockAlignment(messagePointer); break;
		case Y2RCommands::SetInputFormat: setInputFormat(messagePointer); break;
		case Y2RCommands::SetInputLineWidth: setInputLineWidth(messagePointer); break;
		case Y2RCommands::SetInputLines: setInputLines(messagePointer); break;
		case Y2RCommands::SetOutputFormat: setOutputFormat(messagePointer); break;
		case Y2RCommands::SetReceiving: setReceiving(messagePointer); break;
		case Y2RCommands::SetRotation: setRotation(messagePointer); break;
		case Y2RCommands::SetSendingY: setSendingY(messagePointer); break;
		case Y2RCommands::SetSendingU: setSendingU(messagePointer); break;
		case Y2RCommands::SetSendingV: setSendingV(messagePointer); break;
		case Y2RCommands::SetSpacialDithering: setSpacialDithering(messagePointer); break;
		case Y2RCommands::SetStandardCoeff: setStandardCoeff(messagePointer); break;
		case Y2RCommands::SetTemporalDithering: setTemporalDithering(messagePointer); break;
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

void Y2RService::setBlockAlignment(u32 messagePointer) {
	const u32 newAlignment = mem.read32(messagePointer + 4);
	log("Y2R::SetBlockAlignment (format = %d)\n", newAlignment);

	if (newAlignment > 1) {
		Helpers::warn("Warning: Invalid block alignment for Y2R conversion\n");
	} else {
		alignment = static_cast<BlockAlignment>(newAlignment);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x7, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
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

	mem.write32(messagePointer, IPC::responseHeader(0x3, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::setRotation(u32 messagePointer) {
	const u32 rot = mem.read32(messagePointer + 4);
	log("Y2R::SetRotation (format = %d)\n", rot);

	if (rot > 3) {
		Helpers::warn("Warning: Invalid rotation for Y2R conversion\n");
	} else {
		rotation = static_cast<Rotation>(rot);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::setAlpha(u32 messagePointer) {
	alpha = mem.read16(messagePointer + 4);
	log("Y2R::SetAlpha (value = %04X)\n", alpha);

	mem.write32(messagePointer, IPC::responseHeader(0x22, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::setSpacialDithering(u32 messagePointer) {
	const bool enable = mem.read32(messagePointer + 4) != 0;
	log("Y2R::SetSpacialDithering (enable = %d)\n", enable);

	spacialDithering = enable;
	mem.write32(messagePointer, IPC::responseHeader(0x9, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::setTemporalDithering(u32 messagePointer) {
	const bool enable = mem.read32(messagePointer + 4) != 0;
	log("Y2R::SetTemporalDithering (enable = %d)\n", enable);

	temporalDithering = enable;
	mem.write32(messagePointer, IPC::responseHeader(0xB, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::setInputLineWidth(u32 messagePointer) {
	const u16 width = mem.read16(messagePointer + 4);
	log("Y2R::SetInputLineWidth (width = %d)\n", width);

	mem.write32(messagePointer, IPC::responseHeader(0x1A, 1, 0));
	// Width must be > 0, <= 1024 and must be aligned to 8 pixels
	if (width == 0 || width > 1024 || (width & 7) != 0) {
		Helpers::panic("Y2R: Invalid input line width");
	} else {
		inputLineWidth = width;
		mem.write32(messagePointer + 4, Result::Success);
	}
}

void Y2RService::setInputLines(u32 messagePointer) {
	const u16 lines = mem.read16(messagePointer + 4);
	log("Y2R::SetInputLines (lines = %d)\n", lines);
	mem.write32(messagePointer, IPC::responseHeader(0x1C, 1, 0));

	// Width must be > 0, <= 1024 and must be aligned to 8 pixels
	if (lines == 0 || lines > 1024) {
		Helpers::panic("Y2R: Invalid input line count");
	} else {
		// According to Citra, the Y2R module seems to accidentally skip setting the line # if it's 1024
		if (lines != 1024)
			inputLines = lines;
		mem.write32(messagePointer + 4, Result::Success);
	}
}

void Y2RService::setStandardCoeff(u32 messagePointer) {
	const u32 coeff = mem.read32(messagePointer + 4);
	log("Y2R::SetStandardCoeff (coefficient = %d)\n", coeff);
	mem.write32(messagePointer, IPC::responseHeader(0x20, 1, 0));

	if (coeff > 3)
		Helpers::panic("Y2R: Invalid standard coefficient");
	else {
		Helpers::warn("Unimplemented: Y2R standard coefficient");
		mem.write32(messagePointer + 4, Result::Success);
	}
}

void Y2RService::setSendingY(u32 messagePointer) {
	log("Y2R::SetSendingY\n");
	Helpers::warn("Unimplemented Y2R::SetSendingY");

	mem.write32(messagePointer, IPC::responseHeader(0x10, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::setSendingU(u32 messagePointer) {
	log("Y2R::SetSendingU\n");
	Helpers::warn("Unimplemented Y2R::SetSendingU");

	mem.write32(messagePointer, IPC::responseHeader(0x11, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::setSendingV(u32 messagePointer) {
	log("Y2R::SetSendingV\n");
	Helpers::warn("Unimplemented Y2R::SetSendingV");

	mem.write32(messagePointer, IPC::responseHeader(0x12, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::setReceiving(u32 messagePointer) {
	log("Y2R::SetReceiving\n");
	Helpers::warn("Unimplemented Y2R::setReceiving");

	mem.write32(messagePointer, IPC::responseHeader(0x18, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}