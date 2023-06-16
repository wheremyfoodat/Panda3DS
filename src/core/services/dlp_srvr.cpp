#include "services/dlp_srvr.hpp"
#include "ipc.hpp"

namespace DlpSrvrCommands {
	enum : u32 {
		IsChild = 0x000E0040
	};
}

void DlpSrvrService::reset() {}

void DlpSrvrService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case DlpSrvrCommands::IsChild: isChild(messagePointer); break;
		default: Helpers::panic("DLP::SRVR service requested. Command: %08X\n", command);
	}
}

void DlpSrvrService::isChild(u32 messagePointer) {
	log("DLP::SRVR: IsChild\n");

	mem.write32(messagePointer, IPC::responseHeader(0x0E, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0); // We are responsible adults
}