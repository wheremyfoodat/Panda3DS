#include <array>
#include <cctype>
#include <filesystem>
#include <string>
#include <utility>

#include "ipc.hpp"
#include "kernel.hpp"

namespace DirectoryOps {
	enum : u32 {
		Read = 0x08010042,
		Close = 0x08020000,
	};
}

// Helper to convert std::string to an 8.3 filename to mimic how Directory::Read works
using ShortFilename = std::array<char, 9>;
using ShortExtension = std::array<char, 4>;
using Filename83 = std::pair<ShortFilename, ShortExtension>;

// The input string should be the stem and extension together, not separately
// Eg something like "boop.png", "panda.txt", etc
Filename83 convertTo83(const std::string& path) {
	ShortFilename filename;
	ShortExtension extension;

	// Convert a character to add it to the 8.3 name
	// "Characters such as + are changed to the underscore _, and letters are put in uppercase"
	// For now we put letters in uppercase until we find out what is supposed to be converted to _ and so on
	auto convertCharacter = [](char c) { return (char)std::toupper(c); };

	// List of forbidden character for 8.3 filenames, from Citra
	// TODO: Use constexpr when C++20 support is solid
	const std::string forbiddenChars = ".\"/\\[]:;=, ";

	// By default space-initialize the whole name, append null terminator in the end for both the filename and extension
	filename.fill(' ');
	extension.fill(' ');
	filename[filename.size() - 1] = '\0';
	extension[extension.size() - 1] = '\0';

	// Find the position of the dot in the string
	auto dotPos = path.rfind('.');
	// Wikipedia: If a file name has no extension, a trailing . has no effect
	// Thus check if the last character is a dot and ignore it, prefering the previous dot if it exists
	if (dotPos == path.size() - 1) {
		dotPos = path.rfind('.', dotPos);  // Get previous dot
	}

	// If pointPos is not npos we have a valid dot character, and as such an extension
	bool haveExtension = dotPos != std::string::npos;
	int validCharacterCount = 0;
	bool filenameTooBig = false;

	// Parse characters until we're done OR until we reach 9 characters, in which case according to Wikipedia we must truncate to 6 letters
	// And append ~1 in the end
	for (auto c : path.substr(0, dotPos)) {
		// Character is forbidden, we must ignore it
		if (forbiddenChars.find(c) != std::string::npos) {
			continue;
		}

		// We already have capped the amount of characters, thus our filename is too big
		if (validCharacterCount == 8) {
			filenameTooBig = true;
			break;
		}
		filename[validCharacterCount++] = convertCharacter(c);  // Append character to filename
	}

	// Truncate name to 6 characters and denote that it is too big
	// TODO: Wikipedia says we should also do this if the filename contains an invalid character, including spaces. Must test
	if (filenameTooBig) {
		filename[6] = '~';
		filename[7] = '1';
	}

	if (haveExtension) {
		int extensionLen = 0;
		// Copy up to 3 characters from the dot onwards to the extension
		for (auto c : path.substr(dotPos + 1, 3)) {
			extension[extensionLen++] = convertCharacter(c);
		}
	}

	return {filename, extension};
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
	mem.write32(messagePointer, IPC::responseHeader(0x802, 1, 0));
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
		std::filesystem::path filename = path.filename();

		std::filesystem::path relative = path.lexically_relative(dirPath);
		bool isDirectory = std::filesystem::is_directory(relative);

		std::u16string nameU16 = relative.u16string();
		bool isHidden = nameU16[0] == u'.';  // If the first character is a dot then this is a hidden file/folder

		const u32 entryPointer = outPointer + (count * 0x228);  // 0x228 is the size of a single entry
		u32 utfPointer = entryPointer;
		u32 namePointer = entryPointer + 0x20C;
		u32 extensionPointer = entryPointer + 0x216;
		u32 attributePointer = entryPointer + 0x21C;
		u32 sizePointer = entryPointer + 0x220;

		std::string filenameString = filename.string();
		auto [shortFilename, shortExtension] = convertTo83(filenameString);

		for (auto c : nameU16) {
			mem.write16(utfPointer, u16(c));
			utfPointer += sizeof(u16);
		}
		mem.write16(utfPointer, 0);  // Null terminate the UTF16 name

		// Write 8.3 filename-extension
		for (auto c : shortFilename) {
			mem.write8(namePointer, u8(c));
			namePointer += sizeof(u8);
		}

		for (auto c : shortExtension) {
			mem.write8(extensionPointer, u8(c));
			extensionPointer += sizeof(u8);
		}

		mem.write8(outPointer + 0x21A, 1);                            // Always 1 according to 3DBrew
		mem.write8(attributePointer, entry.isDirectory ? 1 : 0);      // "Is directory" attribute
		mem.write8(attributePointer + 1, isHidden ? 1 : 0);           // "Is hidden" attribute
		mem.write8(attributePointer + 2, entry.isDirectory ? 0 : 1);  // "Is archive" attribute
		mem.write8(attributePointer + 3, 0);                          // "Is read-only" attribute

		count++;                  // Increment number of read directories
		session->currentEntry++;  // Increment index of the entry currently being read
	}

	mem.write32(messagePointer, IPC::responseHeader(0x801, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, count);
}
