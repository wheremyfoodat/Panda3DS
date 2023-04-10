#include "services/y2r.hpp"
#include "kernel.hpp"

namespace Y2RCommands {
	enum : u32 {
		SetTransferEndInterrupt = 0x000D0040,
		GetTransferEndEvent = 0x000F0000,
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
}

void Y2RService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case Y2RCommands::DriverInitialize: driverInitialize(messagePointer); break;
		case Y2RCommands::GetTransferEndEvent: getTransferEndEvent(messagePointer); break;
		case Y2RCommands::PingProcess: pingProcess(messagePointer); break;
		case Y2RCommands::SetTransferEndInterrupt: setTransferEndInterrupt(messagePointer); break;
		default: Helpers::panic("Y2R service requested. Command: %08X\n", command);
	}
}

void Y2RService::pingProcess(u32 messagePointer) {
	log("Y2R::PingProcess\n");
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0); // Connected number
}

void Y2RService::driverInitialize(u32 messagePointer) {
	log("Y2R::DriverInitialize\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void Y2RService::getTransferEndEvent(u32 messagePointer) {
	log("Y2R::GetTransferEndEvent\n");
	if (!transferEndEvent.has_value())
		transferEndEvent = kernel.makeEvent(ResetType::OneShot);

	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 12, transferEndEvent.value());
}

void Y2RService::setTransferEndInterrupt(u32 messagePointer) {
	const bool enable = mem.read32(messagePointer + 4) != 0;
	log("Y2R::SetTransferEndInterrupt (enabled: %s)\n", enable ? "yes" : "no");

	mem.write32(messagePointer + 4, Result::Success);
	transferEndInterruptEnabled = enable;
}