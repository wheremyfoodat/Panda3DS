#pragma once
#include <cstring>
#include <optional>
#include "PICA/gpu.hpp"
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

enum class GPUInterrupt : u8 {
	PSC0 = 0, // Memory fill completed
	PSC1 = 1, // ?
	VBlank0 = 2, // ?
	VBlank1 = 3, // ?
	PPF = 4, // Display transfer finished
	P3D = 5, // Command list processing finished
	DMA = 6
};

// More circular dependencies
class Kernel;

class GPUService {
	Handle handle = KernelHandles::GPU;
	Memory& mem;
	GPU& gpu;
	Kernel& kernel;
	u32& currentPID; // Process ID of the current process
	u8* sharedMem; // Pointer to GSP shared memory

	// At any point in time only 1 process has privileges to use rendering functions
	// This is the PID of that process
	u32 privilegedProcess;
	std::optional<Handle> interruptEvent;

	// Number of threads registered via RegisterInterruptRelayQueue
	u32 gspThreadCount = 0;

	MAKE_LOG_FUNCTION(log, gspGPULogger)
	void processCommandBuffer();

	struct FramebufferInfo {
		u32 activeFb;
		u32 leftFramebufferVaddr;
		u32 rightFramebufferVaddr;
		u32 stride;
		u32 format;
		u32 displayFb;
		u32 attribute;
	};
	static_assert(sizeof(FramebufferInfo) == 28, "GSP::GPU::FramebufferInfo has the wrong size");

	struct FramebufferUpdate {
		u8 index;
		u8 dirtyFlag;
		u16 pad0;
		std::array<FramebufferInfo, 2> framebufferInfo;
		u32 pad1;
	};
	static_assert(sizeof(FramebufferUpdate) == 64, "GSP::GPU::FramebufferUpdate has the wrong size");

	// Service commands
	void acquireRight(u32 messagePointer);
	void flushDataCache(u32 messagePointer);
	void registerInterruptRelayQueue(u32 messagePointer);
	void setAxiConfigQoSMode(u32 messagePointer);
	void setBufferSwap(u32 messagePointer);
	void setInternalPriorities(u32 messagePointer);
	void setLCDForceBlack(u32 messagePointer);
	void storeDataCache(u32 messagePointer);
	void triggerCmdReqQueue(u32 messagePointer);
	void writeHwRegs(u32 messagePointer);
	void writeHwRegsWithMask(u32 messagePointer);

	// GSP commands processed via TriggerCmdReqQueue
	void processCommandList(u32* cmd);
	void memoryFill(u32* cmd);
	void triggerDisplayTransfer(u32* cmd);
	void triggerDMARequest(u32* cmd);
	void triggerTextureCopy(u32* cmd);
	void flushCacheRegions(u32* cmd);

	void setBufferSwapImpl(u32 screen_id, const FramebufferInfo& info);

public:
	GPUService(Memory& mem, GPU& gpu, Kernel& kernel, u32& currentPID) : mem(mem), gpu(gpu),
		kernel(kernel), currentPID(currentPID) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
	void requestInterrupt(GPUInterrupt type);
	void setSharedMem(u8* ptr) {
		sharedMem = ptr;
		if (ptr != nullptr) { // Zero-fill shared memory in case the process tries to read stale service data or vice versa
			std::memset(ptr, 0, 0x1000);
		}
	}
};
