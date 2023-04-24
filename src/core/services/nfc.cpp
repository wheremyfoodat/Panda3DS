#include "services/nfc.hpp"
#include "ipc.hpp"

namespace NFCCommands {
	enum : u32 {
		Initialize = 0x00010040
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
	};
}

void NFCService::reset() {}

void NFCService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case NFCCommands::Initialize: initialize(messagePointer); break;
		default: Helpers::panic("NFC service requested. Command: %08X\n", command);
	}
}

void NFCService::initialize(u32 messagePointer) {
	const u8 type = mem.read8(messagePointer + 4);
	log("NFC::Initialize (type = %d)\n", type);

	// TODO: This should error if already initialized. Also sanitize type.
	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}