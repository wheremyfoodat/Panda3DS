#include "services/http.hpp"

#include "ipc.hpp"
#include "result/result.hpp"

namespace HTTPCommands {
	enum : u32 {
		Initialize = 0x00010044,
	};
}

void HTTPService::reset() { initialized = false; }

void HTTPService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case HTTPCommands::Initialize: initialize(messagePointer); break;
		default: Helpers::panic("HTTP service requested. Command: %08X\n", command);
	}
}

void HTTPService::initialize(u32 messagePointer) {
	const u32 postBufferSize = mem.read32(messagePointer + 4);
	const u32 postMemoryBlockHandle = mem.read32(messagePointer + 20);
	log("HTTP::Initialize (POST buffer size = %X, POST buffer memory block handle = %X)\n", postBufferSize, postMemoryBlockHandle);

	mem.write32(messagePointer, IPC::responseHeader(0x01, 1, 0));

	if (initialized) {
		Helpers::warn("HTTP: Tried to initialize service while already initialized");
		// TODO: Error code here
	}

	// 3DBrew: The provided POST buffer must be page-aligned (0x1000).
	if (postBufferSize & 0xfff) {
		Helpers::warn("HTTP: POST buffer size is not page-aligned");
	}

	initialized = true;
	// We currently don't emulate HTTP properly. TODO: Prepare POST buffer here
	mem.write32(messagePointer + 4, Result::Success);
}