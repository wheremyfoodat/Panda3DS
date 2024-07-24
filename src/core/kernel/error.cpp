#include "kernel.hpp"

namespace Commands {
	enum : u32 { Throw = 0x00010800 };
}

namespace FatalErrorType {
	enum : u32 { Generic = 0, Corrupted = 1, CardRemoved = 2, Exception = 3, ResultFailure = 4, Logged = 5 };
}

// HandleType SendSyncRequest targetting the err:f port
void Kernel::handleErrorSyncRequest(u32 messagePointer) {
	u32 cmd = mem.read32(messagePointer);
	switch (cmd) {
		case Commands::Throw: throwError(messagePointer); break;

		default: Helpers::panic("Unimplemented err:f command %08X\n", cmd); break;
	}
}

void Kernel::throwError(u32 messagePointer) {
	const auto type = mem.read8(messagePointer + 4);  // Fatal error type
	const u32 pc = mem.read32(messagePointer + 12);
	const u32 pid = mem.read32(messagePointer + 16);
	logError("Thrown fatal error @ %08X (pid = %X, type = %d)\n", pc, pid, type);

	// Read the error message if type == 4
	if (type == FatalErrorType::ResultFailure) {
		const auto error = mem.readString(messagePointer + 0x24, 0x60);
		logError("ERROR: %s\n", error.c_str());
	}

	Helpers::panic("Thrown fatal error");
}
