#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

// You know the drill
class Kernel;

class NFCService {
	Handle handle = KernelHandles::NFC;
	Memory& mem;
	Kernel& kernel;
	MAKE_LOG_FUNCTION(log, nfcLogger)

	enum class Old3DSAdapterStatus : s32 {
		AttemptingToInitialize = 1,
		InitializationComplete = 2,
		// Other values are errors according to 3DBrew, set the not initialized error to -1 until we find out what it should be
		NotInitialized = -1,
	};

	// Kernel events signaled when an NFC tag goes in and out of range respectively
	std::optional<Handle> tagInRangeEvent, tagOutOfRangeEvent;
	Old3DSAdapterStatus adapterStatus;

	// Service commands
	void communicationGetStatus(u32 messagePointer);
	void initialize(u32 messagePointer);
	void getTagInRangeEvent(u32 messagePointer);
	void getTagOutOfRangeEvent(u32 messagePointer);
	void stopCommunication(u32 messagePointer);

public:
	NFCService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};