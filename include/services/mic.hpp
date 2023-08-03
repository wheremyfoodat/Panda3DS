#pragma once
#include "types.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

class MICService {
	Handle handle = KernelHandles::MIC;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, micLogger)

	// Service commands
	void getGain(u32 messagePointer);
	void mapSharedMem(u32 messagePointer);
	void setClamp(u32 messagePointer);
	void setGain(u32 messagePointer);
	void setIirFilter(u32 messagePointer);
	void setPower(u32 messagePointer);
	void startSampling(u32 messagePointer);
	void theCaptainToadFunction(u32 messagePointer);

	u8 gain = 0; // How loud our microphone input signal is
	bool micEnabled = false;
	bool shouldClamp = false;

public:
	MICService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};