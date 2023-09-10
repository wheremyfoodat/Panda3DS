#include "ipc.hpp"
#include "kernel.hpp"
#include "result/result.hpp"
#include "services/nwm_uds.hpp"

namespace NWMCommands {
	enum : u32 {
		InitializeWithVersion = 0x001B0302,
	};
}

void NwmUdsService::reset() {
	eventHandle = std::nullopt;
	initialized = false;
}

void NwmUdsService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);

	switch (command) {
		case NWMCommands::InitializeWithVersion: initializeWithVersion(messagePointer); break;
		default: Helpers::panic("LCD service requested. Command: %08X\n", command);
	}
}

void NwmUdsService::initializeWithVersion(u32 messagePointer) {
	Helpers::warn("Initializing NWM::UDS (Local multiplayer, unimplemented)\n");
	log("NWM::UDS::InitializeWithVersion\n");

	if (!eventHandle.has_value()) {
		eventHandle = kernel.makeEvent(ResetType::OneShot);
	}

	if (initialized) {
		printf("NWM::UDS initialized twice\n");
	}

	initialized = true;

	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);
	mem.write32(messagePointer + 12, eventHandle.value());
}