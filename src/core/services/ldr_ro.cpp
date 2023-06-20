#include "services/ldr_ro.hpp"
#include "ipc.hpp"

namespace LDRCommands {
	enum : u32 {
		Initialize = 0x000100C2,
		LoadCRR = 0x00020082
	};
}

void LDRService::reset() {}

void LDRService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case LDRCommands::Initialize: initialize(messagePointer); break;
		case LDRCommands::LoadCRR: loadCRR(messagePointer); break;
		default: Helpers::panic("LDR::RO service requested. Command: %08X\n", command);
	}
}

void LDRService::initialize(u32 messagePointer) {
	const u32 crsPointer = mem.read32(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	const u32 mapVaddr = mem.read32(messagePointer + 12);
	const Handle process = mem.read32(messagePointer + 20);

	log("LDR_RO::Initialize (buffer = %08X, size = %08X, vaddr = %08X, process = %X)\n", crsPointer, size, mapVaddr, process);
	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void LDRService::loadCRR(u32 messagePointer) {
	const u32 crrPointer = mem.read32(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	const Handle process = mem.read32(messagePointer + 20);

	log("LDR_RO::LoadCRR (buffer = %08X, size = %08X, process = %X)\n", crrPointer, size, process);
	mem.write32(messagePointer, IPC::responseHeader(0x2, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}