#pragma once

#include <filesystem>
#include <system_error>

#include "helpers.hpp"
#include "mio/mio.hpp"

// Minimal RAII wrapper over memory mapped files

class MemoryMappedFile {
	std::filesystem::path filePath = "";  // path of our file
	mio::mmap_sink map;                   // mmap sink for our file

	u8* pointer = nullptr;  // Pointer to the contents of the memory mapped file
	bool opened = false;

  public:
	bool exists() const { return opened; }
	u8* data() const { return pointer; }

	std::error_code flush();
	MemoryMappedFile();
	MemoryMappedFile(const std::filesystem::path& path);

	~MemoryMappedFile();
	// Returns true on success
	bool open(const std::filesystem::path& path);
	void close();

	// TODO: For memory-mapped output files we'll need some more stuff such as a constructor that takes path/size/shouldCreate as parameters

	u8& operator[](size_t index) { return pointer[index]; }
	const u8& operator[](size_t index) const { return pointer[index]; }

	auto begin() { return map.begin(); }
	auto end() { return map.end(); }
	auto cbegin() { return map.cbegin(); }
	auto cend() { return map.cend(); }

	mio::mmap_sink& getSink() { return map; }
};