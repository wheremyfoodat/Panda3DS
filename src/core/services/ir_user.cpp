#include "ipc.hpp"
#include "services/ir_user.hpp"

namespace IRUserCommands {
	enum : u32 {};
}

void IRUserService::reset() {}

void IRUserService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		default: Helpers::panic("ir:USER service requested. Command: %08X\n", command);
	}
}