#include "services/gsp_gpu.hpp"

// Commands used with SendSyncRequest targetted to the GSP::GPU service
namespace ServiceCommands {
	enum : u32 {
		AcquireRight = 0x00160042,
		RegisterInterruptRelayQueue = 0x00130042,
		WriteHwRegs = 0x00010082,
		WriteHwRegsWithMask = 0x00020084,
		FlushDataCache = 0x00080082,
		SetLCDForceBlack = 0x000B0040,
		TriggerCmdReqQueue = 0x000C0000
	};
}

// Commands written to shared memory and processed by TriggerCmdReqQueue
namespace GPUCommands {
	enum : u32 {
		MemoryFill = 2
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
		case ServiceCommands::AcquireRight: acquireRight(messagePointer); break;
		case ServiceCommands::FlushDataCache: flushDataCache(messagePointer); break;
		case ServiceCommands::RegisterInterruptRelayQueue: registerInterruptRelayQueue(messagePointer); break;
		case ServiceCommands::SetLCDForceBlack: setLCDForceBlack(messagePointer); break;
		case ServiceCommands::TriggerCmdReqQueue: triggerCmdReqQueue(messagePointer); break;
		case ServiceCommands::WriteHwRegs: writeHwRegs(messagePointer); break;
		case ServiceCommands::WriteHwRegsWithMask: writeHwRegsWithMask(messagePointer); break;
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
	// Detect if this function is called a 2nd time because we'll likely need to impl threads properly for the GSP
	static bool beenHere = false;
	if (beenHere) Helpers::panic("RegisterInterruptRelayQueue called a second time. Need to implement GSP threads properly");
	beenHere = true;

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

	u8 index = sharedMem[0]; // The interrupt block is normally located at sharedMem + processGSPIndex*0x40
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

void GPUService::triggerCmdReqQueue(u32 messagePointer) {
	processCommands();
	mem.write32(messagePointer + 4, Result::Success);
}

#include <chrono>
#include <thread>
using namespace std::chrono_literals;
void GPUService::processCommands() {
	if (sharedMem == nullptr) [[unlikely]] { // Shared memory hasn't been set up yet
		return;
	}

	constexpr int threadCount = 1; // TODO: More than 1 thread can have GSP commands at a time
	for (int t = 0; t < threadCount; t++) {
		u8* cmdBuffer = &sharedMem[0x800 + t * 0x200];
		u8& commandsLeft = cmdBuffer[1];
		// Commands start at byte 0x20 of the command buffer, each being 0x20 bytes long
		u32* cmd = reinterpret_cast<u32*>(&cmdBuffer[0x20]);

		printf("Processing %d GPU commands\n", commandsLeft);
		std::this_thread::sleep_for(1000ms);

		while (commandsLeft != 0) {
			u32 cmdID = cmd[0] & 0xff;
			switch (cmdID) {
				case GPUCommands::MemoryFill: memoryFill(cmd); break;
				default: Helpers::panic("GSP::GPU::ProcessCommands: Unknown cmd ID %d", cmdID);
			}

			commandsLeft--;
		}
	}	
}

// Fill 2 GPU framebuffers, buf0 and buf1, using a specific word value
void GPUService::memoryFill(u32* cmd) {
	u32 control = cmd[7];
	
	// buf0 parameters
	u32 start0 = cmd[1]; // Start address for the fill. If 0, don't fill anything
	u32 value0 = cmd[2]; // Value to fill the framebuffer with
	u32 end0 = cmd[3];   // End address for the fill
	u32 control0 = control & 0xffff;

	// buf1 parameters
	u32 start1 = cmd[4];
	u32 value1 = cmd[5];
	u32 end1 = cmd[6];
	u32 control1 = control >> 16;

	if (start0 != 0) {
		gpu.clearBuffer(start0, end0, value0, control0);
	}

	if (start1 != 0) {
		gpu.clearBuffer(start1, end1, value1, control1);
	}
}