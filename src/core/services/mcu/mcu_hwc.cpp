#include "services/mcu/mcu_hwc.hpp"

#include "ipc.hpp"
#include "result/result.hpp"

namespace MCU::HWCCommands {
	enum : u32 {
		GetBatteryLevel = 0x00050000,
		SetInfoLedPattern = 0x000A0640,
	};
}

void MCU::HWCService::reset() {}

void MCU::HWCService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case HWCCommands::GetBatteryLevel: getBatteryLevel(messagePointer); break;
		case HWCCommands::SetInfoLedPattern: setInfoLEDPattern(messagePointer); break;
		default: Helpers::panic("MCU::HWC service requested. Command: %08X\n", command);
	}
}

void MCU::HWCService::getBatteryLevel(u32 messagePointer) {
	log("MCU::HWC::GetBatteryLevel\n");

	mem.write32(messagePointer, IPC::responseHeader(0x5, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, config.batteryPercentage);
}

void MCU::HWCService::setInfoLEDPattern(u32 messagePointer) {
	log("MCU::HWC::SetInfoLedPattern\n");

	// 25 parameters to make some notification LEDs blink...
	mem.write32(messagePointer, IPC::responseHeader(0xA, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}