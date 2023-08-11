#include "ipc.hpp"
#include "result/result.hpp"
#include "services/ssl.hpp"

namespace SSLCommands {
	enum : u32 {
	};
}

void SSLService::reset() {}

void SSLService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		default: Helpers::panic("SSL service requested. Command: %08X\n", command);
	}
}