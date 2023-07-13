#pragma once
#include <optional>

#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

// Circular dependencies in this project? Never
class Kernel;

class IRUserService {
	Handle handle = KernelHandles::IR_USER;
	Memory& mem;
	Kernel& kernel;
	MAKE_LOG_FUNCTION(log, irUserLogger)

	// Service commands
	void disconnect(u32 messagePointer);
	void finalizeIrnop(u32 messagePointer);
	void getConnectionStatusEvent(u32 messagePointer);
	void initializeIrnopShared(u32 messagePointer);
	void requireConnection(u32 messagePointer);

	std::optional<Handle> connectionStatusEvent = std::nullopt;
	std::optional<MemoryBlock> sharedMemory = std::nullopt;
	bool connectedDevice = false;

	// Header of the IR shared memory containing various bits of info
	// https://www.3dbrew.org/wiki/IRUSER_Shared_Memory
	struct SharedMemoryStatus {
		u32 latestReceiveError;
		u32 latestSharedError;

		u8 connectionStatus;
		u8 connectionAttemptStatus;
		u8 connectionRole;
		u8 machineID;
		u8 isConnected;
		u8 networkID;
		u8 isInitialized;  // https://github.com/citra-emu/citra/blob/c10ffda91feb3476a861c47fb38641c1007b9d33/src/core/hle/service/ir/ir_user.cpp#L41
		u8 unk1;
	};
	static_assert(sizeof(SharedMemoryStatus) == 16);

  public:
	IRUserService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};