#include "services/gsp_gpu.hpp"

namespace GPUCommands {
	enum : u32 {
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
	};
}

void GPUService::reset() {}

void GPUService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		default: Helpers::panic("GPU service requested. Command: %08X\n", command);
	}
}