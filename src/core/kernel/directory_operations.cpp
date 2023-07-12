#include <filesystem>

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
	
	const auto p = getObject(directory, KernelObjectType::Directory);
	if (p == nullptr) [[unlikely]] {
		Helpers::panic("Called ReadDirectory on non-existent directory");
	}

	DirectorySession* session = p->getData<DirectorySession>();
	if (!session->pathOnDisk.has_value()) [[unlikely]] {
		Helpers::panic("Called ReadDirectory on directory that doesn't have a path on disk");
	}

	std::filesystem::path dirPath = session->pathOnDisk.value();

	int count = 0;
	while (count < entryCount && session->currentEntry < session->entries.size()) {
		const auto& entry = session->entries[session->currentEntry];
		std::filesystem::path path = entry.path;
		std::filesystem::path extension = path.extension();
		std::filesystem::path relative = path.lexically_relative(dirPath);
		bool isDirectory = std::filesystem::is_directory(relative);

		std::u16string nameU16 = relative.u16string();
		std::string nameString = relative.string();
		std::string extensionString = extension.string();

		const u32 entryPointer = outPointer + (count * 0x228); // 0x228 is the size of a single entry
		u32 utfPointer = entryPointer;
		u32 namePointer = entryPointer + 0x20C;
		u32 extensionPointer = entryPointer + 0x216;
		u32 attributePointer = entryPointer + 0x21C;
		u32 sizePointer = entryPointer + 0x220;

		for (auto c : nameU16) {
			mem.write16(utfPointer, u16(c));
			utfPointer += sizeof(u16);
		}
		mem.write16(utfPointer, 0); // Null terminate the UTF16 name

		for (auto c : nameString) {
			//if (c == '.') continue; // Ignore initial dot

			mem.write8(namePointer, u8(c));
			namePointer += sizeof(u8);
		}
		mem.write8(namePointer, 0); // Null terminate 8.3 name

		for (auto c : extensionString) {
			mem.write8(extensionPointer, u8(c));
			extensionPointer += sizeof(u8);
		}
		mem.write8(extensionPointer, 0); // Null terminate 8.3 extension
		mem.write8(outPointer + 0x21A, 1); // Always 1 according to 3DBrew

		mem.write8(attributePointer, entry.isDirectory ? 1 : 0); // "Is directory" attribute

		count++;                  // Increment number of read directories
		session->currentEntry++;  // Increment index of the entry currently being read
	}

	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, count);
}
