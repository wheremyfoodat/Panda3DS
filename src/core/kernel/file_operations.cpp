#include "kernel.hpp"

namespace FileOps {
	enum : u32 {
		Read = 0x080200C2,
		GetSize = 0x08040000,
		Close = 0x08080000
	};
}

namespace Result {
	enum : u32 {
		Success = 0
	};
}


void Kernel::handleFileOperation(u32 messagePointer, Handle file) {
	const u32 cmd = mem.read32(messagePointer);
	switch (cmd) {
		case FileOps::Close: closeFile(messagePointer, file); break;
		case FileOps::GetSize: getFileSize(messagePointer, file); break;
		case FileOps::Read: readFile(messagePointer, file); break;
		default: Helpers::panic("Unknown file operation: %08X", cmd);
	}
}

void Kernel::closeFile(u32 messagePointer, Handle fileHandle) {
	logFileIO("Closed file %X\n", fileHandle);

	const auto p = getObject(fileHandle, KernelObjectType::File);
	if (p == nullptr) [[unlikely]] {
		Helpers::panic("Called CloseFile on non-existent file");
	}

	p->getData<FileSession>()->isOpen = false;
	mem.write32(messagePointer + 4, Result::Success);
}

void Kernel::readFile(u32 messagePointer, Handle fileHandle) {
	u64 offset = mem.read64(messagePointer + 4);
	u32 size = mem.read32(messagePointer + 12);
	u32 dataPointer = mem.read32(messagePointer + 20);

	logFileIO("Trying to read %X bytes from file %X, starting from offset %llX into memory address %08X\n",
		size, fileHandle, offset, dataPointer);

	const auto p = getObject(fileHandle, KernelObjectType::File);
	if (p == nullptr) [[unlikely]] {
		Helpers::panic("Called ReadFile on non-existent file");
	}

	FileSession* file = p->getData<FileSession>();
	if (!file->isOpen) {
		Helpers::panic("Tried to read closed file");
	}

	auto archive = file->archive;

	std::optional<u32> bytesRead = archive->readFile(file, offset, size, dataPointer);
	if (!bytesRead.has_value()) {
		Helpers::panic("Kernel::ReadFile failed");
	} else {
		mem.write32(messagePointer + 4, Result::Success);
		mem.write32(messagePointer + 8, bytesRead.value());
	}
}

void Kernel::getFileSize(u32 messagePointer, Handle fileHandle) {
	logFileIO("Getting size of file %X\n", fileHandle);

	const auto p = getObject(fileHandle, KernelObjectType::File);
	if (p == nullptr) [[unlikely]] {
		Helpers::panic("Called GetFileSize on non-existent file");
	}

	FileSession* file = p->getData<FileSession>();
	if (!file->isOpen) {
		Helpers::panic("Tried to get size of closed file");
	}

	mem.write32(messagePointer + 4, Result::Success);
	mem.write64(messagePointer + 8, 0); // Size here
	Helpers::panic("TODO: Implement FileOp::GetSize");
}