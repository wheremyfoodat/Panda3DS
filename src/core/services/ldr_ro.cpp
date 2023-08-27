#include "services/ldr_ro.hpp"
#include "ipc.hpp"

namespace LDRCommands {
	enum : u32 {
		Initialize = 0x000100C2,
		LoadCRR = 0x00020082,
		LoadCRONew = 0x000902C2,
	};
}

void LDRService::reset() {}

void LDRService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case LDRCommands::Initialize: initialize(messagePointer); break;
		case LDRCommands::LoadCRR: loadCRR(messagePointer); break;
		case LDRCommands::LoadCRONew: loadCRONew(messagePointer); break;
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

void LDRService::loadCRONew(u32 messagePointer) {
	const u32 croPointer = mem.read32(messagePointer + 4);
	const u32 mapVaddr = mem.read32(messagePointer + 8);
	const u32 size = mem.read32(messagePointer + 12);
	const u32 dataVaddr = mem.read32(messagePointer + 16);
	const u32 dataSize = mem.read32(messagePointer + 24);
	const u32 bssVaddr = mem.read32(messagePointer + 28);
	const u32 bssSize = mem.read32(messagePointer + 32);
	const bool autoLink = mem.read32(messagePointer + 36) != 0;
	const u32 fixLevel = mem.read32(messagePointer + 40);
	const Handle process = mem.read32(messagePointer + 52);

	log("LDR_RO::LoadCRONew (buffer = %08X, vaddr = %08X, size = %08X, .data vaddr = %08X, .data size = %08X, .bss vaddr = %08X, .bss size = %08X, auto link = %d, fix level = %X, process = %X)\n", croPointer, mapVaddr, size, dataVaddr, dataSize, bssVaddr, bssSize, autoLink, fixLevel, process);
	mem.write32(messagePointer, IPC::responseHeader(0x9, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, size);
}