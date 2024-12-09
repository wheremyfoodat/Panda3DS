#include "services/news_s.hpp"

#include "ipc.hpp"

namespace NewsCommands {
	enum : u32 {};
}

void NewsSService::reset() {}

void NewsSService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		default: Helpers::panic("news:s service requested. Command: %08X\n", command);
	}
}