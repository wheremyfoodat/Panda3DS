#include "services/gsp_gpu.hpp"

namespace GPUCommands {
	enum : u32 {
		AcquireRight = 0x00160042,
		RegisterInterruptRelayQueue = 0x00130042,
		WriteHwRegs = 0x00010082,
		WriteHwRegsWithMask = 0x00020084,
		FlushDataCache = 0x00080082,
		SetLCDForceBlack = 0x000B0040
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
		SuccessRegisterIRQ = 0x2A07 // TODO: Is this a reference to the Ricoh 2A07 used in PAL NES systems?
	};
}

void GPUService::reset() {
	privilegedProcess = 0xFFFFFFFF; // Set the privileged process to an invalid handle
	sharedMem = nullptr;
}

void GPUService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case GPUCommands::AcquireRight: acquireRight(messagePointer); break;
		case GPUCommands::FlushDataCache: flushDataCache(messagePointer); break;
		case GPUCommands::RegisterInterruptRelayQueue: registerInterruptRelayQueue(messagePointer); break;
		case GPUCommands::SetLCDForceBlack: setLCDForceBlack(messagePointer); break;
		case GPUCommands::WriteHwRegs: writeHwRegs(messagePointer); break;
		case GPUCommands::WriteHwRegsWithMask: writeHwRegsWithMask(messagePointer); break;
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
	mem.write32(messagePointer + 16, KernelHandles::GSPSharedMemHandle);
}

void GPUService::requestInterrupt(GPUInterrupt type) {
	if (sharedMem == nullptr) [[unlikely]] { // Shared memory hasn't been set up yet
		return;
	}

	u8 index = sharedMem[0];
	u8& interruptCount = sharedMem[1];
	u8 flagIndex = (index + interruptCount) % 0x34;
	interruptCount++;
	
	sharedMem[0xC + flagIndex] = static_cast<u8>(type);
}

void GPUService::writeHwRegs(u32 messagePointer) {
	u32 ioAddr = mem.read32(messagePointer + 4); // GPU address based at 0x1EB00000, word aligned
	const u32 size = mem.read32(messagePointer + 8); // Size in bytes
	u32 dataPointer = mem.read32(messagePointer + 16);
	printf("GSP::GPU::writeHwRegs (GPU address = %08X, size = %X, data address = %08X)\n", ioAddr, size, dataPointer);

	// Check for alignment
	if ((size & 3) || (ioAddr & 3) || (dataPointer & 3)) {
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
		// TODO: Write the value to the register
	}
	mem.write32(messagePointer + 4, Result::Success);
}

// Update sequential GPU registers using an array of data and mask values using this formula
// GPU register = (register & ~mask) | (data & mask).
void GPUService::writeHwRegsWithMask(u32 messagePointer) {
	u32 ioAddr = mem.read32(messagePointer + 4); // GPU address based at 0x1EB00000, word aligned
	const u32 size = mem.read32(messagePointer + 8); // Size in bytes

	u32 dataPointer = mem.read32(messagePointer + 16); // Data pointer
	u32 maskPointer = mem.read32(messagePointer + 24); // Mask pointer

	printf("GSP::GPU::writeHwRegsWithMask (GPU address = %08X, size = %X, data address = %08X, mask address = %08X)\n", 
		ioAddr, size, dataPointer, maskPointer);

	// Check for alignment
	if ((size & 3) || (ioAddr & 3) || (dataPointer & 3) || (maskPointer & 3)) {
		Helpers::panic("GSP::GPU::writeHwRegs misalignment");
	}

	if (size > 0x80) {
		Helpers::panic("GSP::GPU::writeHwRegs size too big");
	}

	if (ioAddr >= 0x420000) {
		Helpers::panic("GSP::GPU::writeHwRegs offset too big");
	}

	for (u32 i = 0; i < size; i += 4) {
		const u32 currentValue = 0; // TODO: Read the actual register value
		const u32 data = mem.read32(dataPointer);
		const u32 mask = mem.read32(maskPointer);

		u32 newValue = (currentValue & ~mask) | (data & mask);
		// TODO: write new value
		maskPointer += 4;
		dataPointer += 4;
		ioAddr += 4;
	}

	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::flushDataCache(u32 messagePointer) {
	u32 address = mem.read32(messagePointer + 4);
	u32 size = mem.read32(messagePointer + 8);
	u32 processHandle = handle = mem.read32(messagePointer + 16);
	printf("GSP::GPU::FlushDataCache(address = %08X, size = %X, process = %X\n", address, size, processHandle);

	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::setLCDForceBlack(u32 messagePointer) {
	u32 flag = mem.read32(messagePointer + 4);
	printf("GSP::GPU::SetLCDForceBlank(flag = %d)\n", flag);

	if (flag != 0) {
		printf("Filled both LCDs with black\n");
	}

	mem.write32(messagePointer + 4, Result::Success);
}