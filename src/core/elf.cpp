#include "memory.hpp"
#include "elfio/elfio.hpp"

using namespace ELFIO;

std::optional<u32> Memory::loadELF(std::filesystem::path& path) {
	elfio reader;
	if (!reader.load(path.string())) {
		printf("Woops failed to load ELF\n");
		return std::nullopt;
	}

    auto seg_num = reader.segments.size();
    printf("Number of segments: %d\n", seg_num);
    for (int i = 0; i < seg_num; ++i) {
        const auto pseg = reader.segments[i];
        std::cout << "  [" << i << "] 0x" << std::hex << pseg->get_flags()
            << "\t0x" << pseg->get_virtual_address() << "\t0x"
            << pseg->get_file_size() << "\t0x" << pseg->get_memory_size()
            << std::endl;
    }

    return static_cast<u32>(reader.get_entry());
}