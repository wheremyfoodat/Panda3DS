#include "memory_mapped_file.hpp"

MemoryMappedFile::MemoryMappedFile() : opened(false), filePath(""), pointer(nullptr) {}
MemoryMappedFile::MemoryMappedFile(const std::filesystem::path& path) { open(path); }
MemoryMappedFile::~MemoryMappedFile() { close(); }

// TODO: This should probably also return the error one way or another eventually
bool MemoryMappedFile::open(const std::filesystem::path& path) {
	std::error_code error;
	map = mio::make_mmap_sink(path.string(), 0, mio::map_entire_file, error);

	if (error) {
		opened = false;
		return false;
	}

	filePath = path;
	pointer = (u8*)map.data();
	opened = true;
	return true;
}

void MemoryMappedFile::close() {
	if (opened) {
		opened = false;
		pointer = nullptr; // Set the pointer to nullptr to avoid errors related to lingering pointers

		map.unmap();
	}
}

std::error_code MemoryMappedFile::flush() {
	std::error_code ret;
	map.sync(ret);

	return ret;
}