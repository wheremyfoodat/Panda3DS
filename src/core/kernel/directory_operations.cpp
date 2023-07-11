#include "kernel.hpp"

namespace DirectoryOps {
	enum : u32 {
		Read = 0x08010042,
		Close = 0x08020000
	};
}

void Kernel::handleDirectoryOperation(u32 messagePointer, Handle directory) {
	const u32 cmd = mem.read32(messagePointer);
	switch (cmd) {
		case DirectoryOps::Close: closeDirectory(messagePointer, directory); break;
		case DirectoryOps::Read: readDirectory(messagePointer, directory); break;
		default: Helpers::panic("Unknown directory operation: %08X", cmd);
	}
}

void Kernel::closeDirectory(u32 messagePointer, Handle directory) {
	logFileIO("Closed directory %X\n", directory);

	const auto p = getObject(directory, KernelObjectType::Directory);
	if (p == nullptr) [[unlikely]] {
		Helpers::panic("Called CloseDirectory on non-existent directory");
	}

	p->getData<DirectorySession>()->isOpen = false;
	mem.write32(messagePointer + 4, Result::Success);
}


void Kernel::readDirectory(u32 messagePointer, Handle directory) {
	const u32 entryCount = mem.read32(messagePointer + 4);
	const u32 outPointer = mem.read32(messagePointer + 12);
	logFileIO("Directory::Read (handle = %X, entry count = %d, out pointer = %08X)\n", directory, entryCount, outPointer);
	Helpers::panicDev("Unimplemented FsDir::Read");

	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);
}
