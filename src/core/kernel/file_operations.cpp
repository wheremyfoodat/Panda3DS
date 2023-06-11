#include "ipc.hpp"
#include "kernel.hpp"

namespace FileOps {
	enum : u32 {
		Read = 0x080200C2,
		Write = 0x08030102,
		GetSize = 0x08040000,
		SetSize = 0x08050080,
		Close = 0x08080000,
		Flush = 0x08090000,
		SetPriority = 0x080A0040,
		OpenLinkFile = 0x080C0000
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
		case FileOps::Flush: flushFile(messagePointer, file); break;
		case FileOps::GetSize: getFileSize(messagePointer, file); break;
		case FileOps::OpenLinkFile: openLinkFile(messagePointer, file); break;
		case FileOps::Read: readFile(messagePointer, file); break;
		case FileOps::SetSize: setFileSize(messagePointer, file); break;
		case FileOps::SetPriority: setFilePriority(messagePointer, file); break;
		case FileOps::Write: writeFile(messagePointer, file); break;
		default: Helpers::panic("Unknown file operation: %08X", cmd);
	}
}

void Kernel::closeFile(u32 messagePointer, Handle fileHandle) {
	logFileIO("Closed file %X\n", fileHandle);

	const auto p = getObject(fileHandle, KernelObjectType::File);
	if (p == nullptr) [[unlikely]] {
		Helpers::panic("Called CloseFile on non-existent file");
	}

	FileSession* session = p->getData<FileSession>();
	session->isOpen = false;
	if (session->fd != nullptr) {
		fclose(session->fd);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x0808, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void Kernel::flushFile(u32 messagePointer, Handle fileHandle) {
	logFileIO("Flushed file %X\n", fileHandle);

	const auto p = getObject(fileHandle, KernelObjectType::File);
	if (p == nullptr) [[unlikely]] {
		Helpers::panic("Called FlushFile on non-existent file");
	}

	FileSession* session = p->getData<FileSession>();
	if (session->fd != nullptr) {
		fflush(session->fd);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x0809, 1, 0));
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

	mem.write32(messagePointer, IPC::responseHeader(0x0802, 2, 2));

	FileSession* file = p->getData<FileSession>();
	if (!file->isOpen) {
		Helpers::panic("Tried to read closed file");
	}
	
	// Handle files with their own file descriptors by just fread'ing the data
	if (file->fd) {
		std::unique_ptr<u8[]> data(new u8[size]);
		IOFile f(file->fd);

		auto [success, bytesRead] = f.readBytes(data.get(), size);

		if (!success) {
			Helpers::panic("Kernel::ReadFile with file descriptor failed");
		}
		else {
			for (size_t i = 0; i < bytesRead; i++) {
				mem.write8(dataPointer + i, data[i]);
			}

			mem.write32(messagePointer + 4, Result::Success);
			mem.write32(messagePointer + 8, bytesRead);
		}

		return;
	}

	// Handle files without their own FD, such as SelfNCCH files
	auto archive = file->archive;
	std::optional<u32> bytesRead = archive->readFile(file, offset, size, dataPointer);
	if (!bytesRead.has_value()) {
		Helpers::panic("Kernel::ReadFile failed");
	} else {
		mem.write32(messagePointer + 4, Result::Success);
		mem.write32(messagePointer + 8, bytesRead.value());
	}
}

void Kernel::writeFile(u32 messagePointer, Handle fileHandle) {
	u64 offset = mem.read64(messagePointer + 4);
	u32 size = mem.read32(messagePointer + 12);
	u32 writeOption = mem.read32(messagePointer + 16);
	u32 dataPointer = mem.read32(messagePointer + 24);

	logFileIO("Trying to write %X bytes to file %X, starting from file offset %llX and memory address %08X\n",
		size, fileHandle, offset, dataPointer);

	const auto p = getObject(fileHandle, KernelObjectType::File);
	if (p == nullptr) [[unlikely]] {
		Helpers::panic("Called ReadFile on non-existent file");
	}

	FileSession* file = p->getData<FileSession>();
	if (!file->isOpen) {
		Helpers::panic("Tried to write closed file");
	}

	if (!file->fd)
		Helpers::panic("[Kernel::File::WriteFile] Tried to write to file without a valid file descriptor");

	std::unique_ptr<u8[]> data(new u8[size]);
	for (size_t i = 0; i < size; i++) {
		data[i] = mem.read8(dataPointer + i);
	}

	IOFile f(file->fd);
	auto [success, bytesWritten] = f.writeBytes(data.get(), size);

	mem.write32(messagePointer, IPC::responseHeader(0x0803, 2, 2));
	if (!success) {
		Helpers::panic("Kernel::WriteFile failed");
	} else {
		mem.write32(messagePointer + 4, Result::Success);
		mem.write32(messagePointer + 8, bytesWritten);
	}
}

void Kernel::setFileSize(u32 messagePointer, Handle fileHandle) {
	logFileIO("Setting size of file %X\n", fileHandle);

	const auto p = getObject(fileHandle, KernelObjectType::File);
	if (p == nullptr) [[unlikely]] {
		Helpers::panic("Called SetFileSize on non-existent file");
	}

	FileSession* file = p->getData<FileSession>();
	if (!file->isOpen) {
		Helpers::panic("Tried to get size of closed file");
	}
	mem.write32(messagePointer, IPC::responseHeader(0x0805, 1, 0));

	if (file->fd) {
		const u64 newSize = mem.read64(messagePointer + 4);
		IOFile f(file->fd);
		bool success = f.setSize(newSize);

		if (success) {
			mem.write32(messagePointer + 4, Result::Success);
		} else {
			Helpers::panic("FileOp::SetFileSize failed");
		}
	} else {
		Helpers::panic("Tried to set file size of file without file descriptor");
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
	mem.write32(messagePointer, IPC::responseHeader(0x0804, 3, 0));

	if (file->fd) {
		IOFile f(file->fd);
		std::optional<u64> size = f.size();

		if (size.has_value()) {
			mem.write32(messagePointer + 4, Result::Success);
			mem.write64(messagePointer + 8, size.value());
		} else {
			Helpers::panic("FileOp::GetFileSize failed");
		}
	} else {
		Helpers::panic("Tried to get file size of file without file descriptor");
	}
}

void Kernel::openLinkFile(u32 messagePointer, Handle fileHandle) {
	logFileIO("Open link file (clone) of file %X\n", fileHandle);

	const auto p = getObject(fileHandle, KernelObjectType::File);
	if (p == nullptr) [[unlikely]] {
		Helpers::panic("Called GetFileSize on non-existent file");
	}

	FileSession* file = p->getData<FileSession>();
	if (!file->isOpen) {
		Helpers::panic("Tried to clone closed file");
	}

	// Make clone object
	auto handle = makeObject(KernelObjectType::File);
	auto& cloneFile = getObjects()[handle];

	// Make a clone of the file by copying the archive/archive path/file path/file descriptor/etc of the original file
	// TODO: Maybe we should duplicate the file handle instead of copying. This way their offsets will be separate
	// However we do seek properly on every file access so this shouldn't matter
	cloneFile.data = new FileSession(*file);

	mem.write32(messagePointer, IPC::responseHeader(0x080C, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 12, handle);
}

void Kernel::setFilePriority(u32 messagePointer, Handle fileHandle) {
	const u32 priority = mem.read32(messagePointer + 4);
	logFileIO("Setting priority of file %X to %d\n", fileHandle, priority);

	const auto p = getObject(fileHandle, KernelObjectType::File);
	if (p == nullptr) [[unlikely]] {
		Helpers::panic("Called GetFileSize on non-existent file");
	}

	FileSession* file = p->getData<FileSession>();
	if (!file->isOpen) {
		Helpers::panic("Tried to clone closed file");
	}
	file->priority = priority;

	mem.write32(messagePointer, IPC::responseHeader(0x080A, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}
