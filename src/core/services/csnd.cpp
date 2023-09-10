#include "services/csnd.hpp"

#include "ipc.hpp"
#include "kernel.hpp"
#include "result/result.hpp"

namespace CSNDCommands {
	enum : u32 {
		Initialize = 0x00010140,
		ExecuteCommands = 0x00030040,
		AcquireSoundChannels = 0x00050000,
	};
}

void CSNDService::reset() {
	csndMutex = std::nullopt;
	initialized = false;
	sharedMemory = nullptr;
	sharedMemSize = 0;
}

void CSNDService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);

	switch (command) {
		case CSNDCommands::AcquireSoundChannels: acquireSoundChannels(messagePointer); break;
		case CSNDCommands::ExecuteCommands: executeCommands(messagePointer); break;
		case CSNDCommands::Initialize: initialize(messagePointer); break;

		default:
			Helpers::warn("Unimplemented CSND service requested. Command: %08X\n", command);
			mem.write32(messagePointer + 4, Result::Success);
			break;
	}
}

void CSNDService::acquireSoundChannels(u32 messagePointer) {
	log("CSND::AcquireSoundChannels\n");
	// The CSND service talks to the DSP using the DSP FIFO to negotiate what CSND channels are allocated to the DSP, and this seems to be channels 0-7 (usually). The rest are dedicated to CSND services.
	// https://www.3dbrew.org/wiki/CSND_Services
	constexpr u32 csndChannelMask = 0xFFFFFF00;

	mem.write32(messagePointer, IPC::responseHeader(0x5, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, csndChannelMask);
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
	sharedMemSize = blockSize;

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 3));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0x4000000);
	mem.write32(messagePointer + 12, csndMutex.value());
	mem.write32(messagePointer + 16, KernelHandles::CSNDSharedMemHandle);
}

void CSNDService::executeCommands(u32 messagePointer) {
	const u32 offset = mem.read32(messagePointer + 4);
	log("CSND::ExecuteCommands (command offset = %X)\n", offset);

	mem.write32(messagePointer, IPC::responseHeader(0x5, 2, 0));

	if (!sharedMemory) {
		Helpers::warn("CSND::Execute commands without shared memory");
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		return;
	}

	mem.write32(messagePointer + 4, Result::Success);

	// This is initially zero when this command data is written by the user process, once the CSND module finishes processing the command this is set
	// to 0x1. This flag is only set to value 1 for the first command(once processing for the entire command chain is finished) at the offset
	// specified in the service command, not all type0 commands in the chain.
	constexpr u32 commandListDoneOffset = 0x4;

	// Make sure to not access OoB of the shared memory block when marking command list processing as finished
	if (offset + commandListDoneOffset < sharedMemSize) {
		sharedMemory[offset + commandListDoneOffset] = 1;
	}
}