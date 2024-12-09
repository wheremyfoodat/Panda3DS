#include "services/news_s.hpp"

#include "ipc.hpp"

namespace NewsCommands {
	enum : u32 {};
}

void NewsSService::reset() {}

void NewsSService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		default:
			mem.write32(messagePointer + 4, Result::Success);
			Helpers::warn("news:s service requested. Command: %08X\n", command);
			break;
	}
}