#include "services/csnd.hpp"

#include "ipc.hpp"
#include "kernel.hpp"
#include "result/result.hpp"

namespace CSNDCommands {
	enum : u32 {
		Initialize = 0x00010140,
	};
}

void CSNDService::reset() {
	csndMutex = std::nullopt;
	initialized = false;
}

void CSNDService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);

	switch (command) {
		case CSNDCommands::Initialize: initialize(messagePointer); break;

		default:
			Helpers::warn("Unimplemented CSND service requested. Command: %08X\n", command);
			mem.write32(messagePointer + 4, Result::Success);
			break;
	}
}

void CSNDService::initialize(u32 messagePointer) {
	u32 blockSize = mem.read32(messagePointer + 4);
	const u32 offset0 = mem.read32(messagePointer + 8);
	const u32 offset1 = mem.read32(messagePointer + 12);
	const u32 offset2 = mem.read32(messagePointer + 16);
	const u32 offset3 = mem.read32(messagePointer + 20);

	log("CSND::Initialize (Block size = %08X, offset0 = %X, offset1 = %X, offset2 = %X, offset3 = %X)\n", blockSize, offset0, offset1, offset2, offset3);

	// Align block size to 4KB. CSND shared memory block is currently stubbed to be 0x3000 == 12KB, so panic if this is more than requested
	blockSize = (blockSize + 0xFFF) & ~0xFFF;
	if (blockSize != 12_KB) {
		Helpers::panic("Unhandled size for CSND shared memory block");
	}

	if (initialized) {
		printf("CSND initialized twice\n");
	}

	if (!csndMutex.has_value()) {
		csndMutex = kernel.makeMutex(false);
	}

	initialized = true;

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 3));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0x4000000);
	mem.write32(messagePointer + 12, csndMutex.value());
	mem.write32(messagePointer + 16, KernelHandles::CSNDSharedMemHandle);
}