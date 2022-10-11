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
		TriggerCmdReqQueue = 0x000C0000,
		SetInternalPriorities = 0x001E0080
	};
}

// Commands written to shared memory and processed by TriggerCmdReqQueue
namespace GXCommands {
	enum : u32 {
		ProcessCommandList = 1,
		MemoryFill = 2,
		TriggerDisplayTransfer = 3,
		FlushCacheRegions = 5
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
		case ServiceCommands::SetInternalPriorities: setInternalPriorities(messagePointer); break;
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
	log("GSP::GPU::AcquireRight (flag = %X, pid = %X)\n", flag, pid);

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
	log("GSP::GPU::RegisterInterruptRelayQueue (flags = %X, event handle = %X)\n", flags, eventHandle);

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
	log("GSP::GPU::writeHwRegs (GPU address = %08X, size = %X, data address = %08X)\n", ioAddr, size, dataPointer);

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

	ioAddr += 0x1EB00000;
	for (u32 i = 0; i < size; i += 4) {
		const u32 value = mem.read32(dataPointer);
		gpu.writeReg(ioAddr, value);
		dataPointer += 4;
		ioAddr += 4;
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

	log("GSP::GPU::writeHwRegsWithMask (GPU address = %08X, size = %X, data address = %08X, mask address = %08X)\n", 
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
	
	ioAddr += 0x1EB00000;
	for (u32 i = 0; i < size; i += 4) {
		const u32 current = gpu.readReg(ioAddr);
		const u32 data = mem.read32(dataPointer);
		const u32 mask = mem.read32(maskPointer);

		u32 newValue = (current & ~mask) | (data & mask);

		gpu.writeReg(ioAddr, newValue);
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
	log("GSP::GPU::FlushDataCache(address = %08X, size = %X, process = %X\n", address, size, processHandle);

	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::setLCDForceBlack(u32 messagePointer) {
	u32 flag = mem.read32(messagePointer + 4);
	log("GSP::GPU::SetLCDForceBlank(flag = %d)\n", flag);

	if (flag != 0) {
		printf("Filled both LCDs with black\n");
	}

	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::triggerCmdReqQueue(u32 messagePointer) {
	processCommandBuffer();
	mem.write32(messagePointer + 4, Result::Success);
}

// Seems to be completely undocumented, probably not very important or useful
void GPUService::setInternalPriorities(u32 messagePointer) {
	log("GSP::GPU::SetInternalPriorities\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::processCommandBuffer() {
	if (sharedMem == nullptr) [[unlikely]] { // Shared memory hasn't been set up yet
		return;
	}

	constexpr int threadCount = 1; // TODO: More than 1 thread can have GSP commands at a time
	for (int t = 0; t < threadCount; t++) {
		u8* cmdBuffer = &sharedMem[0x800 + t * 0x200];
		u8& commandsLeft = cmdBuffer[1];
		// Commands start at byte 0x20 of the command buffer, each being 0x20 bytes long
		u32* cmd = reinterpret_cast<u32*>(&cmdBuffer[0x20]);

		log("Processing %d GPU commands\n", commandsLeft);

		while (commandsLeft != 0) {
			u32 cmdID = cmd[0] & 0xff;
			switch (cmdID) {
				case GXCommands::ProcessCommandList: processCommandList(cmd); break;
				case GXCommands::MemoryFill: memoryFill(cmd); break;
				case GXCommands::TriggerDisplayTransfer: triggerDisplayTransfer(cmd); break;
				case GXCommands::FlushCacheRegions: flushCacheRegions(cmd); break;
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

void GPUService::triggerDisplayTransfer(u32* cmd) {
	log("GSP::GPU::TriggerDisplayTransfer (Stubbed)\n");
	requestInterrupt(GPUInterrupt::PPF); // Send "Display transfer finished" interrupt
}

void GPUService::flushCacheRegions(u32* cmd) {
	log("GSP::GPU::FlushCacheRegions (Stubbed)\n");
}

// Actually send command list (aka display list) to GPU
void GPUService::processCommandList(u32* cmd) {
	u32 address = cmd[1] & ~7; // Buffer address
	u32 size = cmd[2] & ~3; // Buffer size in bytes
	bool updateGas = cmd[3] == 1; // Update gas additive blend results (0 = don't update, 1 = update)
	bool flushBuffer = cmd[7] == 1; // Flush buffer (0 = don't flush, 1 = flush)

	u32* bufferStart = static_cast<u32*>(mem.getReadPointer(address));
	if (!bufferStart) Helpers::panic("Couldn't get buffer for command list");
	// TODO: This is very memory unsafe. We get a pointer to FCRAM and just keep writing without checking if we're gonna go OoB

	u32* curr = bufferStart;
	u32* bufferEnd = bufferStart + (size / sizeof(u32));

	// LUT for converting the parameter mask to an actual 32-bit mask
	// The parameter mask is 4 bits long, each bit corresponding to one byte of the mask
	// If the bit is 0 then the corresponding mask byte is 0, otherwise the mask byte is 0xff
	// So for example if the parameter mask is 0b1001, the full mask is 0xff'00'00'ff
	static constexpr std::array<u32, 16> maskLUT = {
		0x00000000, 0x000000ff, 0x0000ff00, 0x0000ffff, 0x00ff0000, 0x00ff00ff, 0x00ffff00, 0x00ffffff,
		0xff000000, 0xff0000ff, 0xff00ff00, 0xff00ffff, 0xffff0000, 0xffff00ff, 0xffffff00, 0xffffffff,
	};

	while (curr < bufferEnd) {
		// If the buffer is not aligned to an 8 byte boundary, force align it by moving the pointer up a word
		// The curr pointer starts out doubleword-aligned and is increased by 4 bytes each time
		// So to check if it is aligned, we get the number of words it's been incremented by
		// If that number is an odd value then the buffer is not aligned, otherwise it is
		if ((curr - bufferStart) % 2 != 0) {
			curr++;
		}

		// The first word of a command is the command parameter and the second one is the header
		u32 param1 = *curr++;
		u32 header = *curr++;
		
		u32 id = header & 0xffff;
		u32 paramMaskIndex = (header >> 16) & 0xf;
		u32 paramCount = (header >> 20) & 0xff; // Number of additional parameters
		// Bit 31 tells us whether this command is going to write to multiple sequential registers (if the bit is 1)
		// Or if all written values will go to the same register (If the bit is 0). It's essentially the value that
		// gets added to the "id" field after each register write
		bool consecutiveWritingMode = (header >> 31) != 0;

		u32 mask = maskLUT[paramMaskIndex]; // Actual parameter mask
		// Increment the ID by 1 after each write if we're in consecutive mode, or 0 otherwise
		u32 idIncrement = (consecutiveWritingMode) ? 1 : 0;

		gpu.writeInternalReg(id, param1, mask);
		for (u32 i = 0; i < paramCount; i++) {
			id += idIncrement;
			u32 param = *curr++;
			gpu.writeInternalReg(id, param, mask);
		}
	}

	log("GPU::GSP::processCommandList. Address: %08X, size in bytes: %08X\n", address, size);
	requestInterrupt(GPUInterrupt::P3D); // Send an IRQ when command list processing is over
}