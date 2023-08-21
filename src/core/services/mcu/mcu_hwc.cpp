#include "ipc.hpp"
#include "result/result.hpp"
#include "services/mcu/mcu_hwc.hpp"

namespace MCU::HWCCommands {
	enum : u32 {
		GetBatteryLevel = 0x00050000,
	};
}

void MCU::HWCService::reset() {}

void MCU::HWCService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case HWCCommands::GetBatteryLevel: getBatteryLevel(messagePointer); break;
		default: Helpers::panic("MCU::HWC service requested. Command: %08X\n", command);
	}
}

void MCU::HWCService::getBatteryLevel(u32 messagePointer) {
	log("MCU::HWC::GetBatteryLevel\n");

	mem.write32(messagePointer, IPC::responseHeader(0x5, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, config.batteryPercentage);
}