#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

class BOSSService {
	Handle handle = KernelHandles::BOSS;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, bossLogger)

	// Service commands
	void initializeSession(u32 messagePointer);
	void getNsDataIdList(u32 messagePointer);
	void getOptoutFlag(u32 messagePointer);
	void getStorageEntryInfo(u32 messagePointer); // Unknown what this is, name taken from Citra
	void getTaskIdList(u32 messagePointer);
	void getTaskStorageInfo(u32 messagePointer);
	void receiveProperty(u32 messagePointer);
	void registerStorageEntry(u32 messagePointer);
	void unregisterStorage(u32 messagePointer);
	void unregisterTask(u32 messagePointer);

	s8 optoutFlag;
public:
	BOSSService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};