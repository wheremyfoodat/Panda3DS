#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

// Circular dependencies, yay
class Kernel;

class MICService {
	Handle handle = KernelHandles::MIC;
	Memory& mem;
	Kernel& kernel;
	MAKE_LOG_FUNCTION(log, micLogger)

	// Service commands
	void getEventHandle(u32 messagePointer);
	void getGain(u32 messagePointer);
	void getPower(u32 messagePointer);
	void isSampling(u32 messagePointer);
	void mapSharedMem(u32 messagePointer);
	void setClamp(u32 messagePointer);
	void setGain(u32 messagePointer);
	void setIirFilter(u32 messagePointer);
	void setPower(u32 messagePointer);
	void startSampling(u32 messagePointer);
	void stopSampling(u32 messagePointer);
	void unmapSharedMem(u32 messagePointer);
	void theCaptainToadFunction(u32 messagePointer);

	u8 gain = 0; // How loud our microphone input signal is
	bool micEnabled = false;
	bool shouldClamp = false;
	bool currentlySampling = false;

	std::optional<Handle> eventHandle;

public:
	MICService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};