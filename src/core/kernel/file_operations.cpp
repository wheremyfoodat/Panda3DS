#include "kernel.hpp"

namespace FileOps {
	enum : u32 {
		Read = 0x080200C2
	};
}

void Kernel::handleFileOperation(u32 messagePointer, Handle file) {
	const u32 cmd = mem.read32(messagePointer);
	switch (cmd) {
		default: Helpers::panic("Unknown file operation: %08X", cmd);
	}
}

void Kernel::readFile(u32 messagePointer, Handle file) {
	
}