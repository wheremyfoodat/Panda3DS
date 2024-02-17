#include "services/gsp_gpu.hpp"
#include "PICA/regs.hpp"
#include "ipc.hpp"
#include "kernel.hpp"

// Commands used with SendSyncRequest targetted to the GSP::GPU service
namespace ServiceCommands {
	enum : u32 {
		SetAxiConfigQoSMode = 0x00100040,
		AcquireRight = 0x00160042,
		RegisterInterruptRelayQueue = 0x00130042,
		WriteHwRegs = 0x00010082,
		WriteHwRegsWithMask = 0x00020084,
		SetBufferSwap = 0x00050200,
		FlushDataCache = 0x00080082,
		SetLCDForceBlack = 0x000B0040,
		TriggerCmdReqQueue = 0x000C0000,
		ReleaseRight = 0x00170000,
		ImportDisplayCaptureInfo = 0x00180000,
		SaveVramSysArea = 0x00190000,
		RestoreVramSysArea = 0x001A0000,
		SetInternalPriorities = 0x001E0080,
		StoreDataCache = 0x001F0082
	};
}

// Commands written to shared memory and processed by TriggerCmdReqQueue
namespace GXCommands {
	enum : u32 {
		TriggerDMARequest = 0,
		ProcessCommandList = 1,
		MemoryFill = 2,
		TriggerDisplayTransfer = 3,
		TriggerTextureCopy = 4,
		FlushCacheRegions = 5
	};
}

void GPUService::reset() {
	privilegedProcess = 0xFFFFFFFF; // Set the privileged process to an invalid handle
	interruptEvent = std::nullopt;
	gspThreadCount = 0;
	sharedMem = nullptr;
}

void GPUService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case ServiceCommands::TriggerCmdReqQueue: [[likely]] triggerCmdReqQueue(messagePointer); break;
		case ServiceCommands::AcquireRight: acquireRight(messagePointer); break;
		case ServiceCommands::FlushDataCache: flushDataCache(messagePointer); break;
		case ServiceCommands::ImportDisplayCaptureInfo: importDisplayCaptureInfo(messagePointer); break;
		case ServiceCommands::RegisterInterruptRelayQueue: registerInterruptRelayQueue(messagePointer); break;
		case ServiceCommands::ReleaseRight: releaseRight(messagePointer); break;
		case ServiceCommands::RestoreVramSysArea: restoreVramSysArea(messagePointer); break;
		case ServiceCommands::SaveVramSysArea: saveVramSysArea(messagePointer); break;
		case ServiceCommands::SetAxiConfigQoSMode: setAxiConfigQoSMode(messagePointer); break;
		case ServiceCommands::SetBufferSwap: setBufferSwap(messagePointer); break;
		case ServiceCommands::SetInternalPriorities: setInternalPriorities(messagePointer); break;
		case ServiceCommands::SetLCDForceBlack: setLCDForceBlack(messagePointer); break;
		case ServiceCommands::StoreDataCache: storeDataCache(messagePointer); break;
		case ServiceCommands::WriteHwRegs: writeHwRegs(messagePointer); break;
		case ServiceCommands::WriteHwRegsWithMask: writeHwRegsWithMask(messagePointer); break;
		default: Helpers::panic("GPU service requested. Command: %08X\n", command);
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

	mem.write32(messagePointer, IPC::responseHeader(0x16, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::releaseRight(u32 messagePointer) {
	log("GSP::GPU::ReleaseRight\n");
	if (privilegedProcess == currentPID) {
		privilegedProcess = 0xFFFFFFFF;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x17, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// TODO: What is the flags field meant to be?
// What is the "GSP module thread index" meant to be?
// How does the shared memory handle thing work?
void GPUService::registerInterruptRelayQueue(u32 messagePointer) {
	// Detect if this function is called a 2nd time because we'll likely need to impl threads properly for the GSP
	if (gspThreadCount >= 1) {
		Helpers::panic("RegisterInterruptRelayQueue called a second time. Need to implement GSP threads properly");
	}
	gspThreadCount += 1;

	const u32 flags = mem.read32(messagePointer + 4);
	const u32 eventHandle = mem.read32(messagePointer + 12);
	log("GSP::GPU::RegisterInterruptRelayQueue (flags = %X, event handle = %X)\n", flags, eventHandle);

	const auto event = kernel.getObject(eventHandle, KernelObjectType::Event);
	if (event == nullptr) { // Check if interrupt event is invalid
		Helpers::panic("Invalid event passed to GSP::GPU::RegisterInterruptRelayQueue");
	} else {
		interruptEvent = eventHandle;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x13, 2, 2));
	mem.write32(messagePointer + 4, Result::GSP::SuccessRegisterIRQ); // First init returns a unique result
	mem.write32(messagePointer + 8, 0); // TODO: GSP module thread index
	mem.write32(messagePointer + 12, 0); // Translation descriptor
	mem.write32(messagePointer + 16, KernelHandles::GSPSharedMemHandle);
}

void GPUService::requestInterrupt(GPUInterrupt type) {
	// HACK: Signal DSP events on GPU interrupt for now until we have the DSP since games need DSP events
	// Maybe there's a better alternative?
	kernel.signalDSPEvents();

	if (sharedMem == nullptr) [[unlikely]] { // Shared memory hasn't been set up yet
		return;
	}

	// TODO: Add support for multiple GSP threads
	u8 index = sharedMem[0]; // The interrupt block is normally located at sharedMem + processGSPIndex*0x40
	u8& interruptCount = sharedMem[1];
	u8 flagIndex = (index + interruptCount) % 0x34;
	interruptCount++;

	sharedMem[2] = 0; // Set error code to 0
	sharedMem[0xC + flagIndex] = static_cast<u8>(type); // Write interrupt type to queue

	// Update framebuffer info in shared memory
	// Most new games check to make sure that the "flag" byte of the framebuffer info header is set to 0
	// Not emulating this causes Yoshi's Wooly World, Captain Toad, Metroid 2 et al to hang
	if (type == GPUInterrupt::VBlank0 || type == GPUInterrupt::VBlank1) {
		int screen = static_cast<u32>(type) - static_cast<u32>(GPUInterrupt::VBlank0); // 0 for top screen, 1 for bottom
		FramebufferUpdate* update = getFramebufferInfo(screen);

		if (update->dirtyFlag & 1) {
			setBufferSwapImpl(screen, update->framebufferInfo[update->index]);
			update->dirtyFlag &= ~1;
		}
	}

	// Signal interrupt event
	if (interruptEvent.has_value()) {
		kernel.signalEvent(interruptEvent.value());
	}
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

	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
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

	mem.write32(messagePointer, IPC::responseHeader(0x2, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::flushDataCache(u32 messagePointer) {
	u32 address = mem.read32(messagePointer + 4);
	u32 size = mem.read32(messagePointer + 8);
	u32 processHandle = handle = mem.read32(messagePointer + 16);
	log("GSP::GPU::FlushDataCache(address = %08X, size = %X, process = %X)\n", address, size, processHandle);

	mem.write32(messagePointer, IPC::responseHeader(0x8, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::storeDataCache(u32 messagePointer) {
	u32 address = mem.read32(messagePointer + 4);
	u32 size = mem.read32(messagePointer + 8);
	u32 processHandle = handle = mem.read32(messagePointer + 16);
	log("GSP::GPU::StoreDataCache(address = %08X, size = %X, process = %X)\n", address, size, processHandle);

	mem.write32(messagePointer, IPC::responseHeader(0x1F, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::setLCDForceBlack(u32 messagePointer) {
	u32 flag = mem.read32(messagePointer + 4);
	log("GSP::GPU::SetLCDForceBlank(flag = %d)\n", flag);

	if (flag != 0) {
		printf("Filled both LCDs with black\n");
	}

	mem.write32(messagePointer, IPC::responseHeader(0xB, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::triggerCmdReqQueue(u32 messagePointer) {
	processCommandBuffer();
	mem.write32(messagePointer, IPC::responseHeader(0xC, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// Seems to be completely undocumented, probably not very important or useful
void GPUService::setAxiConfigQoSMode(u32 messagePointer) {
	log("GSP::GPU::SetAxiConfigQoSMode\n");
	mem.write32(messagePointer, IPC::responseHeader(0x10, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::setBufferSwap(u32 messagePointer) {
	FramebufferInfo info{};
	const u32 screenId = mem.read32(messagePointer + 4);  // Selects either PDC0 or PDC1
	info.activeFb = mem.read32(messagePointer + 8);
	info.leftFramebufferVaddr = mem.read32(messagePointer + 12);
	info.rightFramebufferVaddr = mem.read32(messagePointer + 16);
	info.stride = mem.read32(messagePointer + 20);
	info.format = mem.read32(messagePointer + 24);
	info.displayFb = mem.read32(messagePointer + 28);  // Selects either framebuffer A or B

	log("GSP::GPU::SetBufferSwap\n");
	Helpers::warn("Untested GSP::GPU::SetBufferSwap call");

	setBufferSwapImpl(screenId, info);
	mem.write32(messagePointer, IPC::responseHeader(0x05, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// Seems to also be completely undocumented
void GPUService::setInternalPriorities(u32 messagePointer) {
	log("GSP::GPU::SetInternalPriorities\n");
	mem.write32(messagePointer, IPC::responseHeader(0x1E, 1, 0));
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
			const u32 cmdID = cmd[0] & 0xff;
			switch (cmdID) {
				case GXCommands::ProcessCommandList: processCommandList(cmd); break;
				case GXCommands::MemoryFill: memoryFill(cmd); break;
				case GXCommands::TriggerDisplayTransfer: triggerDisplayTransfer(cmd); break;
				case GXCommands::TriggerDMARequest: triggerDMARequest(cmd); break;
				case GXCommands::TriggerTextureCopy: triggerTextureCopy(cmd); break;
				case GXCommands::FlushCacheRegions: flushCacheRegions(cmd); break;
				default: Helpers::panic("GSP::GPU::ProcessCommands: Unknown cmd ID %d", cmdID);
			}

			commandsLeft--;
		}
	}
}

static u32 VaddrToPaddr(u32 addr) {
	if (addr >= VirtualAddrs::VramStart && addr < (VirtualAddrs::VramStart + VirtualAddrs::VramSize)) [[likely]] {
		return addr - VirtualAddrs::VramStart + PhysicalAddrs::VRAM;
	}

	else if (addr >= VirtualAddrs::LinearHeapStartOld && addr < VirtualAddrs::LinearHeapEndOld) {
		return addr - VirtualAddrs::LinearHeapStartOld + PhysicalAddrs::FCRAM;
	}

	else if (addr >= VirtualAddrs::LinearHeapStartNew && addr < VirtualAddrs::LinearHeapEndNew) {
		return addr - VirtualAddrs::LinearHeapStartNew + PhysicalAddrs::FCRAM;
	}

	else if (addr == 0) {
		return 0;
	}

	Helpers::warn("[GSP::GPU VaddrToPaddr] Unknown virtual address %08X", addr);
	// Obviously garbage address
	return 0xF3310932;
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
		gpu.clearBuffer(VaddrToPaddr(start0), VaddrToPaddr(end0), value0, control0);
		requestInterrupt(GPUInterrupt::PSC0);
	}

	if (start1 != 0) {
		gpu.clearBuffer(VaddrToPaddr(start1), VaddrToPaddr(end1), value1, control1);
		requestInterrupt(GPUInterrupt::PSC1);
	}
}

void GPUService::triggerDisplayTransfer(u32* cmd) {
	const u32 inputAddr = VaddrToPaddr(cmd[1]);
	const u32 outputAddr = VaddrToPaddr(cmd[2]);
	const u32 inputSize = cmd[3];
	const u32 outputSize = cmd[4];
	const u32 flags = cmd[5];

	log("GSP::GPU::TriggerDisplayTransfer (Stubbed)\n");
	gpu.displayTransfer(inputAddr, outputAddr, inputSize, outputSize, flags);
	requestInterrupt(GPUInterrupt::PPF); // Send "Display transfer finished" interrupt
}

void GPUService::triggerDMARequest(u32* cmd) {
	const u32 source = cmd[1];
	const u32 dest = cmd[2];
	const u32 size = cmd[3];
	const bool flush = cmd[7] == 1;

	log("GSP::GPU::TriggerDMARequest (source = %08X, dest = %08X, size = %08X)\n", source, dest, size);
	gpu.fireDMA(dest, source, size);
	requestInterrupt(GPUInterrupt::DMA);
}

void GPUService::flushCacheRegions(u32* cmd) {
	log("GSP::GPU::FlushCacheRegions (Stubbed)\n");
}

void GPUService::setBufferSwapImpl(u32 screenId, const FramebufferInfo& info) {
	using namespace PICA::ExternalRegs;

	static constexpr std::array<u32, 8> fbAddresses = {
		Framebuffer0AFirstAddr,
		Framebuffer0BFirstAddr,
		Framebuffer1AFirstAddr,
		Framebuffer1BFirstAddr,
		Framebuffer0ASecondAddr,
		Framebuffer0BSecondAddr,
		Framebuffer1ASecondAddr,
		Framebuffer1BSecondAddr,
	};

	auto& regs = gpu.getExtRegisters();

	const u32 fbIndex = info.activeFb * 4 + screenId * 2;
	regs[fbAddresses[fbIndex]] = VaddrToPaddr(info.leftFramebufferVaddr);
	regs[fbAddresses[fbIndex + 1]] = VaddrToPaddr(info.rightFramebufferVaddr);

	static constexpr std::array<u32, 6> configAddresses = {
		Framebuffer0Config,
		Framebuffer0Select,
		Framebuffer0Stride,
		Framebuffer1Config,
		Framebuffer1Select,
		Framebuffer1Stride,
	};

	const u32 configIndex = screenId * 3;
	regs[configAddresses[configIndex]] = info.format;
	regs[configAddresses[configIndex + 1]] = info.displayFb;
	regs[configAddresses[configIndex + 2]] = info.stride;
}

// Actually send command list (aka display list) to GPU
void GPUService::processCommandList(u32* cmd) {
	const u32 address = cmd[1] & ~7; // Buffer address
	const u32 size = cmd[2] & ~3; // Buffer size in bytes
	[[maybe_unused]] const bool updateGas = cmd[3] == 1; // Update gas additive blend results (0 = don't update, 1 = update)
	[[maybe_unused]] const bool flushBuffer = cmd[7] == 1; // Flush buffer (0 = don't flush, 1 = flush)

	log("GPU::GSP::processCommandList. Address: %08X, size in bytes: %08X\n", address, size);
	gpu.startCommandList(address, size);
	requestInterrupt(GPUInterrupt::P3D); // Send an IRQ when command list processing is over
}

// TODO: Emulate the transfer engine & its registers
// Then this can be emulated by just writing the appropriate values there
void GPUService::triggerTextureCopy(u32* cmd) {
	const u32 inputAddr = VaddrToPaddr(cmd[1]);
	const u32 outputAddr = VaddrToPaddr(cmd[2]);
	const u32 totalBytes = cmd[3];
	const u32 inputSize = cmd[4];
	const u32 outputSize = cmd[5];
	const u32 flags = cmd[6];

	log("GSP::GPU::TriggerTextureCopy (Stubbed)\n");
	gpu.textureCopy(inputAddr, outputAddr, totalBytes, inputSize, outputSize, flags);
	// This uses the transfer engine and thus needs to fire a PPF interrupt.
	// NSMB2 relies on this
	requestInterrupt(GPUInterrupt::PPF);
}

// Used when transitioning from the app to an OS applet, such as software keyboard, mii maker, mii selector, etc
// Stubbed until we decide to support LLE applets
void GPUService::saveVramSysArea(u32 messagePointer) {
	Helpers::warn("GSP::GPU::SaveVramSysArea (stubbed)");

	mem.write32(messagePointer, IPC::responseHeader(0x19, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void GPUService::restoreVramSysArea(u32 messagePointer) {
	Helpers::warn("GSP::GPU::RestoreVramSysArea (stubbed)");

	mem.write32(messagePointer, IPC::responseHeader(0x1A, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// Used in similar fashion to the SaveVramSysArea function
void GPUService::importDisplayCaptureInfo(u32 messagePointer) {
	Helpers::warn("GSP::GPU::ImportDisplayCaptureInfo (stubbed)");

	mem.write32(messagePointer, IPC::responseHeader(0x18, 9, 0));
	mem.write32(messagePointer + 4, Result::Success);

	if (sharedMem == nullptr) {
		Helpers::warn("GSP::GPU::ImportDisplayCaptureInfo called without GSP module being properly initialized!");
		return;
	}

	FramebufferUpdate* topScreen = getTopFramebufferInfo();
	FramebufferUpdate* bottomScreen = getBottomFramebufferInfo();

	// Capture the relevant data for both screens and return them to the caller
	CaptureInfo topScreenCapture = {
		.leftFramebuffer = topScreen->framebufferInfo[topScreen->index].leftFramebufferVaddr,
		.rightFramebuffer = topScreen->framebufferInfo[topScreen->index].rightFramebufferVaddr,
		.format = topScreen->framebufferInfo[topScreen->index].format,
		.stride = topScreen->framebufferInfo[topScreen->index].stride,
	};

	CaptureInfo bottomScreenCapture = {
		.leftFramebuffer = bottomScreen->framebufferInfo[bottomScreen->index].leftFramebufferVaddr,
		.rightFramebuffer = bottomScreen->framebufferInfo[bottomScreen->index].rightFramebufferVaddr,
		.format = bottomScreen->framebufferInfo[bottomScreen->index].format,
		.stride = bottomScreen->framebufferInfo[bottomScreen->index].stride,
	};

	mem.write32(messagePointer + 8, topScreenCapture.leftFramebuffer);
	mem.write32(messagePointer + 12, topScreenCapture.rightFramebuffer);
	mem.write32(messagePointer + 16, topScreenCapture.format);
	mem.write32(messagePointer + 20, topScreenCapture.stride);

	mem.write32(messagePointer + 24, bottomScreenCapture.leftFramebuffer);
	mem.write32(messagePointer + 28, bottomScreenCapture.rightFramebuffer);
	mem.write32(messagePointer + 32, bottomScreenCapture.format);
	mem.write32(messagePointer + 36, bottomScreenCapture.stride);
}
