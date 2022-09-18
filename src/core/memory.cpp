#include "memory.hpp"
#include "config_mem.hpp"
#include <cassert>

using namespace KernelMemoryTypes;

Memory::Memory() {
	fcram = new uint8_t[FCRAM_SIZE]();
	readTable.resize(totalPageCount, 0);
	writeTable.resize(totalPageCount, 0);
	memoryInfo.reserve(32); // Pre-allocate some room for memory allocation info to avoid dynamic allocs
}

void Memory::reset() {
	// Unallocate all memory
	memoryInfo.clear();
	usedFCRAMPages.reset();
	usedUserMemory = 0_MB;

	for (u32 i = 0; i < totalPageCount; i++) {
		readTable[i] = 0;
		writeTable[i] = 0;
	}

	// Map stack pages as R/W
	// We have 16KB for the stack, so we allocate the last 16KB of APPLICATION FCRAM for the stack
	u32 basePaddrForStack = FCRAM_APPLICATION_SIZE - VirtualAddrs::StackSize;
	allocateMemory(VirtualAddrs::StackBottom, basePaddrForStack, VirtualAddrs::StackSize, true);

	// And map 4KB of FCRAM before the stack for TLS
	u32 basePaddrForTLS = basePaddrForStack - VirtualAddrs::TLSSize;
	allocateMemory(VirtualAddrs::TLSBase, basePaddrForTLS, VirtualAddrs::TLSSize, true);
}

std::optional<u32> Memory::allocateMemory(u32 vaddr, u32 paddr, u32 size, bool linear, bool r, bool w, bool x) {
	// Kernel-allocated memory & size must always be aligned to a page boundary
	// Additionally assert we don't OoM and that we don't try to allocate physical FCRAM past what's available to userland
	assert(isAligned(vaddr) && isAligned(paddr) && isAligned(size));
	assert(size <= FCRAM_APPLICATION_SIZE);
	assert(usedUserMemory + size <= FCRAM_APPLICATION_SIZE);
	assert(paddr + size <= FCRAM_APPLICATION_SIZE);

	// Amount of available user FCRAM pages and FCRAM pages to allocate respectively
	const u32 availablePageCount = (FCRAM_APPLICATION_SIZE - usedUserMemory) / pageSize;
	const u32 neededPageCount = size / pageSize;

	assert(availablePageCount >= neededPageCount);

	// If the vaddr is 0 that means we need to select our own
	// Depending on whether our mapping should be linear or not we allocate from one of the 2 typical heap spaces
	// We don't plan on implementing freeing any time soon, so we can pick added userUserMemory to the vaddr base to
	// Get the full vaddr.
	// TODO: Fix this
	if (vaddr == 0) {
		vaddr = usedUserMemory + (linear ? VirtualAddrs::LinearHeapStart : VirtualAddrs::NormalHeapStart);
	}

	usedUserMemory += size;

	// If the paddr is 0, that means we need to select our own
	// TODO: Fix. This method always tries to allocate blocks linearly.
	// However, if the allocation is non-linear, the panic will trigger when it shouldn't.
	// Non-linear allocation needs special handling
	if (paddr == 0) {
		std::optional<u32> newPaddr = findPaddr(size);
		if (!newPaddr.has_value())
			Helpers::panic("Failed to find paddr");

		paddr = newPaddr.value();
		assert(paddr + size <= FCRAM_APPLICATION_SIZE);
	}

	// Do linear mapping
	u32 virtualPage = vaddr >> pageShift;
	u32 physPage = paddr >> pageShift; // TODO: Special handle when non-linear mapping is necessary
	for (u32 i = 0; i < neededPageCount; i++) {
		if (r) {
			readTable[virtualPage] = uintptr_t(&fcram[physPage * pageSize]);
		}
		if (w) {
			writeTable[virtualPage] = uintptr_t(&fcram[physPage * pageSize]);
		}

		// Mark FCRAM page as allocated and go on
		usedFCRAMPages[physPage] = true;
		virtualPage++;
		physPage++;
	}

	// Back up the info for this allocation in our memoryInfo vector
	u32 perms = (r ? PERMISSION_R : 0) | (w ? PERMISSION_W : 0) | (x ? PERMISSION_X : 0);
	memoryInfo.push_back(std::move(MemoryInfo(vaddr, size, perms, KernelMemoryTypes::Reserved)));

	return vaddr;
}

// Find a paddr which we can use for allocating "size" bytes
std::optional<u32> Memory::findPaddr(u32 size) {
	const u32 neededPages = size / pageSize;

	// The FCRAM page we're testing to see if it's appropriate to use
	u32 candidatePage = 0;
	// The number of linear available pages we could find starting from this candidate page.
	// If this ends up >= than neededPages then the paddr is good (ie we can use the candidate page as a base address)
	u32 counter = 0;

	for (u32 i = 0; i < FCRAM_PAGE_COUNT; i++) {
		if (usedFCRAMPages[i]) { // Page is occupied already, go to new candidate
			candidatePage = i + 1;
			counter = 0;
		}
		else { // Our candidate page has 1 mor 
			counter++;
			// Check if there's enough free memory to use this page
			if (counter == neededPages) {
				return candidatePage * pageSize;
			}
		}
	}

	// Couldn't find any page :(
	return std::nullopt;
}

u8 Memory::read8(u32 vaddr) {
	const u32 page = vaddr >> pageShift;
	const u32 offset = vaddr & pageMask;

	uintptr_t pointer = readTable[page];
	if (pointer != 0) [[likely]] {
		return *(u8*)(pointer + offset);
	}
	else {
		switch (vaddr) {
			case ConfigMem::KernelVersionMinor: return 38;
			default: Helpers::panic("Unimplemented 8-bit read, addr: %08X", vaddr);
		}
	}
}

u16 Memory::read16(u32 vaddr) {
	Helpers::panic("Unimplemented 16-bit read, addr: %08X", vaddr);
}

u32 Memory::read32(u32 vaddr) {
	const u32 page = vaddr >> pageShift;
	const u32 offset = vaddr & pageMask;

	uintptr_t pointer = readTable[page];
	if (pointer != 0) [[likely]] {
		return *(u32*)(pointer + offset);
	} else {
		Helpers::panic("Unimplemented 32-bit read, addr: %08X", vaddr);
	}
}

u64 Memory::read64(u32 vaddr) {
	u64 bottom = u64(read32(vaddr));
	u64 top = u64(read32(vaddr + 4));
	return (top << 32) | bottom;
}

void Memory::write8(u32 vaddr, u8 value) {
	const u32 page = vaddr >> pageShift;
	const u32 offset = vaddr & pageMask;

	uintptr_t pointer = writeTable[page];
	if (pointer != 0) [[likely]] {
		*(u8*)(pointer + offset) = value;
	}
	else {
		Helpers::panic("Unimplemented 8-bit write, addr: %08X, val: %02X", vaddr, value);
	}
}

void Memory::write16(u32 vaddr, u16 value) {
	Helpers::panic("Unimplemented 16-bit write, addr: %08X, val: %04X", vaddr, value);
}

void Memory::write32(u32 vaddr, u32 value) {
	const u32 page = vaddr >> pageShift;
	const u32 offset = vaddr & pageMask;

	uintptr_t pointer = writeTable[page];
	if (pointer != 0) [[likely]] {
		*(u32*)(pointer + offset) = value;
	} else {
		Helpers::panic("Unimplemented 32-bit write, addr: %08X, val: %08X", vaddr, value);
	}
}

void Memory::write64(u32 vaddr, u64 value) {
	write32(vaddr, u32(value));
	write32(vaddr + 4, u32(value >> 32));
}

void* Memory::getReadPointer(u32 address) {
	const u32 page = address >> pageShift;
	const u32 offset = address & pageMask;

	uintptr_t pointer = readTable[page];
	if (pointer == 0) return nullptr;
	return (void*)(pointer + offset);
}

void* Memory::getWritePointer(u32 address) {
	const u32 page = address >> pageShift;
	const u32 offset = address & pageMask;

	uintptr_t pointer = writeTable[page];
	if (pointer == 0) return nullptr;
	return (void*)(pointer + offset);
}

// Thank you Citra devs
std::string Memory::readString(u32 address, u32 maxSize) {
	std::string string;
	string.reserve(maxSize);
	
	for (std::size_t i = 0; i < maxSize; ++i) {
		char c = read8(address++);
		if (c == '\0')
			break;
		string.push_back(c);
	}
	string.shrink_to_fit();

	return string;
}

// The way I understand how the kernel's QueryMemory is supposed to work is that you give it a vaddr
// And the kernel looks up the memory allocations it's performed, finds which one it belongs in and returns its info?
// TODO: Verify this
MemoryInfo Memory::queryMemory(u32 vaddr) {
	// Check each allocation
	for (auto& alloc : memoryInfo) {
		// Check if the memory address belongs in this allocation and return the info if so
		if (vaddr >= alloc.baseVaddr && vaddr < alloc.end()) {
			return alloc;
		}
	}

	// Otherwise, if this vaddr was never allocated
	return MemoryInfo(vaddr, 0, 0, KernelMemoryTypes::Free);
}