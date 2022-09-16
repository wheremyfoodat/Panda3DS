#include "memory.hpp"

#include <cstring>
#include "elfio/elfio.hpp"

using namespace ELFIO;

std::optional<u32> Memory::loadELF(std::ifstream& file) {
	elfio reader;
	if (!file.good() || !reader.load(file)) {
		printf("Woops failed to load ELF\n");
		return std::nullopt;
	}

    auto segNum = reader.segments.size();
    printf("Number of segments: %d\n", segNum);
    printf(" #  Perms       Vaddr           File Size       Mem Size\n");
    for (int i = 0; i < segNum; ++i) {
        const auto seg = reader.segments[i];
        const auto flags = seg->get_flags();
        const u32 vaddr = static_cast<u32>(seg->get_virtual_address()); // Vaddr the segment is loaded in
        u32 fileSize = static_cast<u32>(seg->get_file_size()); // Size of segment in file
        u32 memorySize = static_cast<u32>(seg->get_memory_size()); // Size of segment in memory
        u8* data = (u8*)seg->get_data();

        // Get read/write/execute permissions for segment
        const bool r = (flags & 0b100) != 0;
        const bool w = (flags & 0b010) != 0;
        const bool x = (flags & 0b001) != 0;

        printf("[%d] (%c%c%c)\t%08X\t%08X\t%08X\n", i, r ? 'r' : '-', w ? 'w' : '-', x ? 'x' : '-', vaddr, fileSize, memorySize);
    
        // Assert that the segment will be loaded in the executable region. If it isn't then panic.
        // The executable region starts at 0x00100000 and has a maximum size of 0x03F00000
        u64 endAddress = (u64)vaddr + (u64)fileSize;
        const bool isGood = vaddr >= VirtualAddrs::ExecutableStart && endAddress < VirtualAddrs::ExecutableEnd;
        if (!isGood) {
            Helpers::panic("ELF is loaded at invalid place");
            return std::nullopt;
        }

        if (memorySize & pageMask) {
            // Round up the size of the ELF segment to a page (4KB) boundary, as the OS can only alloc this way
            memorySize = (memorySize + pageSize - 1) & -pageSize;
            Helpers::warn("Rounding ELF segment size to %08X\n", memorySize);
        }

        u32 fcramAddr = findPaddr(memorySize).value();
        std::memcpy(&fcram[fcramAddr], data, fileSize);

        // Allocate the segment on the OS side
        allocateMemory(vaddr, fcramAddr, memorySize, true);
    }

    return static_cast<u32>(reader.get_entry());
}