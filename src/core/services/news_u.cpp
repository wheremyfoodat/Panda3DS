#include "ipc.hpp"
#include "services/news_u.hpp"

namespace NewsCommands {
	enum : u32 {};
}

void NewsUService::reset() {}

void NewsUService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		default: Helpers::panic("news:u service requested. Command: %08X\n", command);
	}
}