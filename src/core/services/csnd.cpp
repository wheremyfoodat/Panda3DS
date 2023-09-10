#include "services/csnd.hpp"

#include "ipc.hpp"
#include "result/result.hpp"

void CSNDService::reset() {}

void CSNDService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);

	switch (command) {
		default:
			Helpers::panic("Unimplemented CSND service requested. Command: %08X\n", command);
			break;
	}
}