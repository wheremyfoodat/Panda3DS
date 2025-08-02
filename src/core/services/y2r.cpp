#include "services/y2r.hpp"

#include "ipc.hpp"
#include "kernel.hpp"

namespace Y2RCommands {
	enum : u32 {
		SetInputFormat = 0x00010040,
		SetOutputFormat = 0x00030040,
		GetOutputFormat = 0x00040000,
		SetRotation = 0x00050040,
		SetBlockAlignment = 0x00070040,
		GetBlockAlignment = 0x00080000,
		SetSpacialDithering = 0x00090040,
		SetTemporalDithering = 0x000B0040,
		SetTransferEndInterrupt = 0x000D0040,
		GetTransferEndEvent = 0x000F0000,
		SetSendingY = 0x00100102,
		SetSendingU = 0x00110102,
		SetSendingV = 0x00120102,
		SetSendingYUV = 0x00130102,
		IsFinishedSendingYUV = 0x00140000,
		IsFinishedSendingY = 0x00150000,
		IsFinishedSendingU = 0x00160000,
		IsFinishedSendingV = 0x00170000,
		SetReceiving = 0x00180102,
		IsFinishedReceiving = 0x00190000,
		SetInputLineWidth = 0x001A0040,
		GetInputLineWidth = 0x001B0000,
		SetInputLines = 0x001C0040,
		GetInputLines = 0x001D0000,
		SetCoefficientParams = 0x001E0100,
		GetCoefficientParams = 0x001F0000,
		SetStandardCoeff = 0x00200040,
		GetStandardCoefficientParams = 0x00210040,
		SetAlpha = 0x00220040,
		StartConversion = 0x00260000,
		StopConversion = 0x00270000,
		IsBusyConversion = 0x00280000,
		SetPackageParameter = 0x002901C0,
		PingProcess = 0x002A0000,
		DriverInitialize = 0x002B0000,
		DriverFinalize = 0x002C0000,
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

	conversionCoefficients.fill(0);
	isBusy = false;
}

void Y2RService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case Y2RCommands::DriverInitialize: driverInitialize(messagePointer); break;
		case Y2RCommands::DriverFinalize: driverFinalize(messagePointer); break;
		case Y2RCommands::GetBlockAlignment: getBlockAlignment(messagePointer); break;
		case Y2RCommands::GetInputLines: getInputLines(messagePointer); break;
		case Y2RCommands::GetInputLineWidth: getInputLineWidth(messagePointer); break;
		case Y2RCommands::GetOutputFormat: getOutputFormat(messagePointer); break;
		case Y2RCommands::GetTransferEndEvent: getTransferEndEvent(messagePointer); break;
		case Y2RCommands::GetStandardCoefficientParams: getStandardCoefficientParams(messagePointer); break;
		case Y2RCommands::IsBusyConversion: isBusyConversion(messagePointer); break;
		case Y2RCommands::IsFinishedReceiving: isFinishedReceiving(messagePointer); break;
		case Y2RCommands::IsFinishedSendingY: isFinishedSendingY(messagePointer); break;
		case Y2RCommands::IsFinishedSendingU: isFinishedSendingU(messagePointer); break;
		case Y2RCommands::IsFinishedSendingV: isFinishedSendingV(messagePointer); break;
		case Y2RCommands::IsFinishedSendingYUV: isFinishedSendingYUV(messagePointer); break;
		case Y2RCommands::PingProcess: pingProcess(messagePointer); break;
		case Y2RCommands::SetAlpha: setAlpha(messagePointer); break;
		case Y2RCommands::SetBlockAlignment: setBlockAlignment(messagePointer); break;
		case Y2RCommands::SetInputFormat: setInputFormat(messagePointer); break;
		case Y2RCommands::SetInputLineWidth: setInputLineWidth(messagePointer); break;
		case Y2RCommands::SetInputLines: setInputLines(messagePointer); break;
		case Y2RCommands::SetOutputFormat: setOutputFormat(messagePointer); break;
		case Y2RCommands::SetPackageParameter: setPackageParameter(messagePointer); break;
		case Y2RCommands::SetReceiving: setReceiving(messagePointer); break;
		case Y2RCommands::SetRotation: setRotation(messagePointer); break;
		case Y2RCommands::SetSendingY: setSendingY(messagePointer); break;
		case Y2RCommands::SetSendingU: setSendingU(messagePointer); break;
		case Y2RCommands::SetSendingV: setSendingV(messagePointer); break;
		case Y2RCommands::SetSendingYUV: setSendingYUV(messagePointer); break;
		case Y2RCommands::SetSpacialDithering: setSpacialDithering(messagePointer); break;
		case Y2RCommands::SetStandardCoeff: setStandardCoeff(messagePointer); break;
		case Y2RCommands::SetTemporalDithering: setTemporalDithering(messagePointer); break;
		case Y2RCommands::SetTransferEndInterrupt: setTransferEndInterrupt(messagePointer); break;
		case Y2RCommands::StartConversion: [[likely]] startConversion(messagePointer); break;
		case Y2RCommands::StopConversion: stopConversion(messagePointer); break;

		// Intentionally break ordering a bit for less-used Y2R functions
		case Y2RCommands::SetCoefficientParams: setCoefficientParams(messagePointer); break;
		case Y2RCommands::GetCoefficientParams: getCoefficientParams(messagePointer); break;
		default: Helpers::panic("Y2R service requested. Command: %08X\n", command);
	}
}

void Y2RService::pingProcess(u32 messagePointer) {
	log("Y2R::PingProcess\n");
	mem.write32(messagePointer, IPC::responseHeader(0x2A, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);  // Connected number
}

void Y2RService::driverInitialize(u32 messagePointer) {
	log("Y2R::DriverInitialize\n");
	mem.write32(messagePointer, IPC::responseHeader(0x2B, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);

	conversionCoefficients.fill(0);
}

void Y2RService::driverFinalize(u32 messagePointer) {
	log("Y2R::DriverInitialize\n");
	mem.write32(messagePointer, IPC::responseHeader(0x2C, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::getTransferEndEvent(u32 messagePointer) {
	log("Y2R::GetTransferEndEvent\n");
	if (!transferEndEvent.has_value()) {
		transferEndEvent = kernel.makeEvent(ResetType::OneShot);
	}

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

	if (isBusy) {
		isBusy = false;
		kernel.getScheduler().removeEvent(Scheduler::EventType::SignalY2R);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x27, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// See above. Our Y2R conversion (when implemented) will be instant because there's really no point trying to delay it
// This is a modern enough console for us to screw timings
void Y2RService::isBusyConversion(u32 messagePointer) {
	log("Y2R::IsBusyConversion\n");

	mem.write32(messagePointer, IPC::responseHeader(0x28, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, static_cast<u32>(isBusy ? BusyStatus::Busy : BusyStatus::NotBusy));
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

void Y2RService::getBlockAlignment(u32 messagePointer) {
	log("Y2R::GetBlockAlignment\n");

	mem.write32(messagePointer, IPC::responseHeader(0x8, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, static_cast<u32>(alignment));
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

void Y2RService::getOutputFormat(u32 messagePointer) {
	log("Y2R::GetOutputFormat\n");

	mem.write32(messagePointer, IPC::responseHeader(0x4, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, static_cast<u32>(outputFmt));
}

void Y2RService::setPackageParameter(u32 messagePointer) {
	// Package parameter is 3 words
	const u32 word1 = mem.read32(messagePointer + 4);
	const u32 word2 = mem.read32(messagePointer + 8);
	const u32 word3 = mem.read32(messagePointer + 12);
	Helpers::warn("Y2R::SetPackageParameter\n");

	mem.write32(messagePointer, IPC::responseHeader(0x29, 1, 0));
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

void Y2RService::getInputLineWidth(u32 messagePointer) {
	log("Y2R::GetInputLineWidth\n");

	mem.write32(messagePointer, IPC::responseHeader(0x1B, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, inputLineWidth);
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
		if (lines != 1024) {
			inputLines = lines;
		}
		mem.write32(messagePointer + 4, Result::Success);
	}
}

void Y2RService::getInputLines(u32 messagePointer) {
	log("Y2R::GetInputLines\n");

	mem.write32(messagePointer, IPC::responseHeader(0x1D, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, inputLines);
}

void Y2RService::setStandardCoeff(u32 messagePointer) {
	const u32 coeff = mem.read32(messagePointer + 4);
	log("Y2R::SetStandardCoeff (coefficient = %d)\n", coeff);
	mem.write32(messagePointer, IPC::responseHeader(0x20, 1, 0));

	if (coeff > 3) {  // Invalid coefficient, should have an error code
		Helpers::panic("Y2R: Invalid standard coefficient (coefficient = %d)\n", coeff);
	}

	else {
		Helpers::warn("Unimplemented: Y2R standard coefficient");
		mem.write32(messagePointer + 4, Result::Success);
	}
}

void Y2RService::getStandardCoefficientParams(u32 messagePointer) {
	const u32 coefficientIndex = mem.read32(messagePointer + 4);
	log("Y2R::GetStandardCoefficientParams (coefficient = %d)\n", coefficientIndex);

	if (coefficientIndex > 3) {  // Invalid coefficient, should have an error code
		Helpers::panic("Y2R: Invalid standard coefficient (coefficient = %d)\n", coefficientIndex);
	} else {
		mem.write32(messagePointer, IPC::responseHeader(0x21, 5, 0));
		mem.write32(messagePointer + 4, Result::Success);
		const auto& coeff = standardCoefficients[coefficientIndex];

		// Write standard coefficient parameters to output buffer
		for (int i = 0; i < 8; i++) {
			const u32 pointer = messagePointer + 8 + i * sizeof(u16);  // Pointer to write parameter to
			mem.write16(pointer, coeff[i]);
		}
	}
}

void Y2RService::setCoefficientParams(u32 messagePointer) {
	log("Y2R::SetCoefficientParams\n");
	auto& coeff = conversionCoefficients;

	// Write coefficient parameters to output buffer
	for (int i = 0; i < 8; i++) {
		const u32 pointer = messagePointer + 4 + i * sizeof(u16);  // Pointer to write parameter to
		coeff[i] = mem.read16(pointer);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x1E, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::getCoefficientParams(u32 messagePointer) {
	log("Y2R::GetCoefficientParams\n");
	mem.write32(messagePointer, IPC::responseHeader(0x1F, 5, 0));
	mem.write32(messagePointer + 4, Result::Success);
	const auto& coeff = conversionCoefficients;

	// Write coefficient parameters to output buffer
	for (int i = 0; i < 8; i++) {
		const u32 pointer = messagePointer + 8 + i * sizeof(u16);  // Pointer to write parameter to
		mem.write16(pointer, coeff[i]);
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

void Y2RService::setSendingYUV(u32 messagePointer) {
	log("Y2R::SetSendingYUV\n");
	Helpers::warn("Unimplemented Y2R::SetSendingYUV");

	mem.write32(messagePointer, IPC::responseHeader(0x13, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::setReceiving(u32 messagePointer) {
	log("Y2R::SetReceiving\n");
	Helpers::warn("Unimplemented Y2R::setReceiving");

	mem.write32(messagePointer, IPC::responseHeader(0x18, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::startConversion(u32 messagePointer) {
	log("Y2R::StartConversion\n");

	// TODO: Actually launch conversion here
	mem.write32(messagePointer, IPC::responseHeader(0x26, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);

	// Schedule Y2R conversion end event.
	// The tick value is tweaked based on the minimum delay needed to get FIFA 15 to not hang due to a race condition on its title screen
	static constexpr u64 delayTicks = 1'350'000;
	isBusy = true;

	// Remove any potential pending Y2R event and schedule a new one
	Scheduler& scheduler = kernel.getScheduler();
	scheduler.removeEvent(Scheduler::EventType::SignalY2R);
	scheduler.addEvent(Scheduler::EventType::SignalY2R, scheduler.currentTimestamp + delayTicks);
}

void Y2RService::isFinishedSendingYUV(u32 messagePointer) {
	log("Y2R::IsFinishedSendingYUV");
	constexpr bool finished = true;  // For now, Y2R transfers are instant

	mem.write32(messagePointer, IPC::responseHeader(0x14, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, finished ? 1 : 0);
}

void Y2RService::isFinishedSendingY(u32 messagePointer) {
	log("Y2R::IsFinishedSendingY");
	constexpr bool finished = true;

	mem.write32(messagePointer, IPC::responseHeader(0x15, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, finished ? 1 : 0);
}

void Y2RService::isFinishedSendingU(u32 messagePointer) {
	log("Y2R::IsFinishedSendingU");
	constexpr bool finished = true;

	mem.write32(messagePointer, IPC::responseHeader(0x16, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, finished ? 1 : 0);
}

void Y2RService::isFinishedSendingV(u32 messagePointer) {
	log("Y2R::IsFinishedSendingV");
	constexpr bool finished = true;

	mem.write32(messagePointer, IPC::responseHeader(0x17, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, finished ? 1 : 0);
}

void Y2RService::isFinishedReceiving(u32 messagePointer) {
	log("Y2R::IsFinishedSendingReceiving");
	constexpr bool finished = true;  // For now, receiving components is also instant

	mem.write32(messagePointer, IPC::responseHeader(0x17, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, finished ? 1 : 0);
}

void Y2RService::signalConversionDone() {
	if (isBusy) {
		isBusy = false;

		// Signal the transfer end event if it's been created. TODO: Is this affected by SetTransferEndInterrupt?
		if (transferEndEvent.has_value()) {
			kernel.signalEvent(transferEndEvent.value());
		}
	}
}