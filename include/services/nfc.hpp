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

	enum class Old3DSAdapterStatus : u32 {
		Idle = 0,
		AttemptingToInitialize = 1,
		InitializationComplete = 2,
		Active = 3,
	};

	enum class TagStatus : u8 {
		NotInitialized = 0,
		Initialized = 1,
		Scanning = 2,
		InRange = 3,
		OutOfRange = 4,
		Loaded = 5,
	};

	// Kernel events signaled when an NFC tag goes in and out of range respectively
	std::optional<Handle> tagInRangeEvent, tagOutOfRangeEvent;

	Old3DSAdapterStatus adapterStatus;
	TagStatus tagStatus;
	bool initialized = false;

	// Service commands
	void communicationGetResult(u32 messagePointer);
	void communicationGetStatus(u32 messagePointer);
	void initialize(u32 messagePointer);
	void getTagInRangeEvent(u32 messagePointer);
	void getTagOutOfRangeEvent(u32 messagePointer);
	void getTagState(u32 messagePointer);
	void startCommunication(u32 messagePointer);
	void stopCommunication(u32 messagePointer);

public:
	NFCService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};