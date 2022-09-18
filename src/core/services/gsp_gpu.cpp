#include "services/gsp_gpu.hpp"

namespace GPUCommands {
	enum : u32 {
		AcquireRight = 0x00160042,
		RegisterInterruptRelayQueue = 0x00130042,
		WriteHwRegs = 0x00010082
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
		SuccessRegisterIRQ = 0x2A07
	};
}

void GPUService::reset() {
	privilegedProcess = 0xFFFFFFFF; // Set the privileged process to an invalid handle
}

void GPUService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case GPUCommands::AcquireRight: acquireRight(messagePointer); break;
		case GPUCommands::RegisterInterruptRelayQueue: registerInterruptRelayQueue(messagePointer); break;
		case GPUCommands::WriteHwRegs: writeHwRegs(messagePointer); break;
;		default: Helpers::panic("GPU service requested. Command: %08X\n", command);
	}
}

void GPUService::acquireRight(u32 messagePointer) {
	const u32 flag = mem.read32(messagePointer + 4);
	const u32 pid = mem.read32(messagePointer + 12);
	printf("GSP::GPU::AcquireRight (flag = %X, pid = %X)\n", flag, pid);

	if (flag != 0) {
		Helpers::panic("GSP::GPU::acquireRight with flag != 0 needs to perform additional initialization");
	}

	if (pid == KernelHandles::CurrentProcess) {
		privilegedProcess = currentPID;
	} else {
		privilegedProcess = pid;
	}

	mem.write32(messagePointer + 4, Result::Success);
}

// TODO: What is the flags field meant to be?
// What is the "GSP module thread index" meant to be?
// How does the shared memory handle thing work?
void GPUService::registerInterruptRelayQueue(u32 messagePointer) {
	const u32 flags = mem.read32(messagePointer + 4);
	const u32 eventHandle = mem.read32(messagePointer + 12);
	printf("GSP::GPU::RegisterInterruptRelayQueue (flags = %X, event handle = %X)\n", flags, eventHandle);

	mem.write32(messagePointer + 4, Result::SuccessRegisterIRQ);
	mem.write32(messagePointer + 8, 0); // TODO: GSP module thread index
	mem.write32(messagePointer + 12, 0); // Translation descriptor
	mem.write32(messagePointer + 16, 0); // TODO: shared memory handle
}

void GPUService::writeHwRegs(u32 messagePointer) {
	u32 ioAddr = mem.read32(messagePointer + 4); // GPU address based at 0x1EB00000, word aligned
	const u32 size = mem.read32(messagePointer + 8); // Size in bytes
	u32 dataPointer = mem.read32(messagePointer + 16);
	printf("GSP::GPU::writeHwRegs (GPU address = %08X, size = %X, data address = %08X)\n", ioAddr, size, dataPointer);

	// Check for alignment
	if ((size & 3) || (ioAddr & 3)) {
		Helpers::panic("GSP::GPU::writeHwRegs misalignment");
	}

	if (size > 0x80) {
		Helpers::panic("GSP::GPU::writeHwRegs size too big");
	}

	if (ioAddr >= 0x420000) {
		Helpers::panic("GSP::GPU::writeHwRegs offset too big");
	}

	for (u32 i = 0; i < size; i += 4) {
		const u32 value = mem.read32(dataPointer);
		printf("GSP::GPU: Wrote %08X to GPU register %X\n", value, ioAddr);
		dataPointer += 4;
		ioAddr += 4;
	}
	mem.write32(messagePointer + 4, Result::Success);
}