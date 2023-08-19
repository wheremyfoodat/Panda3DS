#include "ipc.hpp"
#include "result/result.hpp"
#include "services/mcu/mcu_hwc.hpp"

namespace MCU::HWCCommands {
	enum : u32 {
	};
}

void MCU::HWCService::reset() {}

void MCU::HWCService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		default: Helpers::panic("MCU::HWC service requested. Command: %08X\n", command);
	}
}