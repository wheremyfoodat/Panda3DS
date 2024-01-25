#include "services/ptm.hpp"
#include "ipc.hpp"

namespace PTMCommands {
	enum : u32 {
		GetAdapterState = 0x00050000,
		GetBatteryLevel = 0x00070000,
		GetBatteryChargeState = 0x00080000,
		GetPedometerState = 0x00090000,
		GetStepHistory = 0x000B00C2,
		GetTotalStepCount = 0x000C0000,
		GetStepHistoryAll = 0x000F0084,
		ConfigureNew3DSCPU = 0x08180040,

		// ptm:play functions
		GetPlayHistory = 0x08070082,
		GetPlayHistoryStart = 0x08080000,
		GetPlayHistoryLength = 0x08090000,
		CalcPlayHistoryStart = 0x080B0080,
	};
}

void PTMService::reset() {}

void PTMService::handleSyncRequest(u32 messagePointer, PTMService::Type type) {
	const u32 command = mem.read32(messagePointer);

	// ptm:play functions
	switch (command) {
			case PTMCommands::ConfigureNew3DSCPU: configureNew3DSCPU(messagePointer); break;
			case PTMCommands::GetAdapterState: getAdapterState(messagePointer); break;
			case PTMCommands::GetBatteryChargeState: getBatteryChargeState(messagePointer); break;
			case PTMCommands::GetBatteryLevel: getBatteryLevel(messagePointer); break;
			case PTMCommands::GetPedometerState: getPedometerState(messagePointer); break;
			case PTMCommands::GetStepHistory: getStepHistory(messagePointer); break;
			case PTMCommands::GetStepHistoryAll: getStepHistoryAll(messagePointer); break;
			case PTMCommands::GetTotalStepCount: getTotalStepCount(messagePointer); break;

			default:
				// ptm:play-only functions
				if (type == Type::PLAY) {
					switch (command) {
						case PTMCommands::GetPlayHistory:
						case PTMCommands::GetPlayHistoryStart:
						case PTMCommands::GetPlayHistoryLength:
							mem.write32(messagePointer + 4, Result::Success);
							mem.write64(messagePointer + 8, 0);
							Helpers::warn("Stubbed PTM:PLAY service requested. Command: %08X\n", command);
							break;

						default: Helpers::panic("PTM PLAY service requested. Command: %08X\n", command); break;
					}
				} else {
					Helpers::panic("PTM service requested. Command: %08X\n", command);
				}
		}
}

void PTMService::getAdapterState(u32 messagePointer) {
	log("PTM::GetAdapterState\n");

	mem.write32(messagePointer, IPC::responseHeader(0x5, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, config.chargerPlugged ? 1 : 0);
}

void PTMService::getBatteryChargeState(u32 messagePointer) {
	log("PTM::GetBatteryChargeState");
	// We're only charging if the battery is not already full
	const bool charging = config.chargerPlugged && (config.batteryPercentage < 100);

	mem.write32(messagePointer, IPC::responseHeader(0x8, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, charging ? 1 : 0);
}

void PTMService::getPedometerState(u32 messagePointer) {
	log("PTM::GetPedometerState");
	const bool countingSteps = true;

	mem.write32(messagePointer, IPC::responseHeader(0x9, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, countingSteps ? 1 : 0);
}

void PTMService::getBatteryLevel(u32 messagePointer) {
	log("PTM::GetBatteryLevel");

	mem.write32(messagePointer, IPC::responseHeader(0x7, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, batteryPercentToLevel(config.batteryPercentage));
}

void PTMService::getStepHistory(u32 messagePointer) {
	log("PTM::GetStepHistory [stubbed]\n");
	mem.write32(messagePointer, IPC::responseHeader(0xB, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
}

void PTMService::getStepHistoryAll(u32 messagePointer) {
	log("PTM::GetStepHistoryAll [stubbed]\n");
	mem.write32(messagePointer, IPC::responseHeader(0xF, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
}

void PTMService::getTotalStepCount(u32 messagePointer) {
	log("PTM::GetTotalStepCount\n");
	mem.write32(messagePointer, IPC::responseHeader(0xC, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 3); // We walk a lot
}

void PTMService::configureNew3DSCPU(u32 messagePointer) {
	log("PTM::ConfigureNew3DSCPU [stubbed]\n");
	mem.write32(messagePointer, IPC::responseHeader(0x818, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}