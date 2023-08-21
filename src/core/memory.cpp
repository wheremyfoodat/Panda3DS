#include "memory.hpp"

#include <cassert>
#include <chrono>  // For time since epoch
#include <cmrc/cmrc.hpp>
#include <ctime>

#include "config_mem.hpp"
#include "resource_limits.hpp"
#include "services/ptm.hpp"

CMRC_DECLARE(ConsoleFonts);

using namespace KernelMemoryTypes;

Memory::Memory(u64& cpuTicks, const EmulatorConfig& config) : cpuTicks(cpuTicks), config(config) {
	fcram = new uint8_t[FCRAM_SIZE]();
	dspRam = new uint8_t[DSP_RAM_SIZE]();

	readTable.resize(totalPageCount, 0);
	writeTable.resize(totalPageCount, 0);
	memoryInfo.reserve(32);  // Pre-allocate some room for memory allocation info to avoid dynamic allocs
}

void Memory::reset() {
	// Unallocate all memory
	memoryInfo.clear();
	usedFCRAMPages.reset();
	usedUserMemory = u32(0_MB);
	usedSystemMemory = u32(0_MB);

	for (u32 i = 0; i < totalPageCount; i++) {
		readTable[i] = 0;
		writeTable[i] = 0;
	}

	// Map (32 * 4) KB of FCRAM before the stack for the TLS of each thread
	std::optional<u32> tlsBaseOpt = findPaddr(32 * 4_KB);
	if (!tlsBaseOpt.has_value()) {  // Should be unreachable but still good to have
		Helpers::panic("Failed to allocate memory for thread-local storage");
	}

	u32 basePaddrForTLS = tlsBaseOpt.value();
	for (u32 i = 0; i < appResourceLimits.maxThreads; i++) {
		u32 vaddr = VirtualAddrs::TLSBase + i * VirtualAddrs::TLSSize;
		allocateMemory(vaddr, basePaddrForTLS, VirtualAddrs::TLSSize, true);
		basePaddrForTLS += VirtualAddrs::TLSSize;
	}

	// Initialize shared memory blocks and reserve memory for them
	for (auto& e : sharedMemBlocks) {
		if (e.handle == KernelHandles::FontSharedMemHandle) {
			// Read font size from the cmrc filesystem the font is stored in
			auto fonts = cmrc::ConsoleFonts::get_filesystem();
			e.size = fonts.open("CitraSharedFontUSRelocated.bin").size();
		}

		e.mapped = false;
		e.paddr = allocateSysMemory(e.size);
	}

	// Map DSP RAM as R/W at [0x1FF00000, 0x1FF7FFFF]
	constexpr u32 dspRamPages = DSP_RAM_SIZE / pageSize;               // Number of DSP RAM pages
	constexpr u32 initialPage = VirtualAddrs::DSPMemStart / pageSize;  // First page of DSP RAM in the virtual address space

	for (u32 i = 0; i < dspRamPages; i++) {
		auto pointer = uintptr_t(&dspRam[i * pageSize]);

		readTable[i + initialPage] = pointer;
		writeTable[i + initialPage] = pointer;
	}

	// Later adjusted based on ROM header when possible
	region = Regions::USA;
}

bool Memory::allocateMainThreadStack(u32 size) {
	// Map stack pages as R/W
	std::optional<u32> basePaddr = findPaddr(size);
	if (!basePaddr.has_value()) {  // Should also be unreachable but still good to have
		return false;
	}

	const u32 stackBottom = VirtualAddrs::StackTop - size;
	std::optional<u32> result = allocateMemory(stackBottom, basePaddr.value(), size, true);  // Should never be nullopt
	return result.has_value();
}

u8 Memory::read8(u32 vaddr) {
	const u32 page = vaddr >> pageShift;
	const u32 offset = vaddr & pageMask;

	uintptr_t pointer = readTable[page];
	if (pointer != 0) [[likely]] {
		return *(u8*)(pointer + offset);
	} else {
		switch (vaddr) {
			case ConfigMem::BatteryState: {
				// Set by the PTM module
				// Charger plugged: Shows whether the charger is plugged
				// Charging: Shows whether the charger is plugged and the console is actually charging, ie the battery is not full
				// BatteryLevel: A battery level calculated via PTM::GetBatteryLevel
				// These are all assembled into a bitfield and returned via config memory
				const bool chargerPlugged = config.chargerPlugged;
				const bool charging = config.chargerPlugged && (config.batteryPercentage < 100);
				const auto batteryLevel = static_cast<BatteryLevel>(PTMService::batteryPercentToLevel(config.batteryPercentage));

				return getBatteryState(chargerPlugged, charging, batteryLevel);
			}
			case ConfigMem::EnvInfo: return envInfo;
			case ConfigMem::HardwareType: return ConfigMem::HardwareCodes::Product;
			case ConfigMem::KernelVersionMinor: return u8(kernelVersion & 0xff);
			case ConfigMem::KernelVersionMajor: return u8(kernelVersion >> 8);
			case ConfigMem::LedState3D: return 1;    // Report the 3D LED as always off (non-zero) for now
			case ConfigMem::NetworkState: return 2;  // Report that we've got an internet connection
			case ConfigMem::HeadphonesConnectedMaybe: return 0;
			case ConfigMem::Unknown1086: return 1;  // It's unknown what this is but some games want it to be 1

			case ConfigMem::FirmUnknown: return firm.unk;
			case ConfigMem::FirmRevision: return firm.revision;
			case ConfigMem::FirmVersionMinor: return firm.minor;
			case ConfigMem::FirmVersionMajor: return firm.major;

			default: Helpers::panic("Unimplemented 8-bit read, addr: %08X", vaddr);
		}
	}
}

u16 Memory::read16(u32 vaddr) {
	const u32 page = vaddr >> pageShift;
	const u32 offset = vaddr & pageMask;

	uintptr_t pointer = readTable[page];
	if (pointer != 0) [[likely]] {
		return *(u16*)(pointer + offset);
	} else {
		switch (vaddr) {
			case ConfigMem::WifiMac + 4: return 0xEEFF;  // Wifi MAC: Last 2 bytes of MAC Address
			default: Helpers::panic("Unimplemented 16-bit read, addr: %08X", vaddr);
		}
	}
}

u32 Memory::read32(u32 vaddr) {
	const u32 page = vaddr >> pageShift;
	const u32 offset = vaddr & pageMask;

	uintptr_t pointer = readTable[page];
	if (pointer != 0) [[likely]] {
		return *(u32*)(pointer + offset);
	} else {
		switch (vaddr) {
			case ConfigMem::Datetime0: return u32(timeSince3DSEpoch());  // ms elapsed since Jan 1 1900, bottom 32 bits
			case ConfigMem::Datetime0 + 4:
				return u32(timeSince3DSEpoch() >> 32);  // top 32 bits
			// Ticks since time was last updated. For now we return the current tick count
			case ConfigMem::Datetime0 + 8: return u32(cpuTicks);
			case ConfigMem::Datetime0 + 12: return u32(cpuTicks >> 32);
			case ConfigMem::Datetime0 + 16: return 0xFFB0FF0;  // Unknown, set by PTM
			case ConfigMem::Datetime0 + 20:
			case ConfigMem::Datetime0 + 24:
			case ConfigMem::Datetime0 + 28: return 0;  // Set to 0 by PTM

			case ConfigMem::AppMemAlloc: return appResourceLimits.maxCommit;
			case ConfigMem::SyscoreVer: return 2;
			case 0x1FF81000: return 0;                   // TODO: Figure out what this config mem address does
			case ConfigMem::WifiMac: return 0xFF07F440;  // Wifi MAC: First 4 bytes of MAC Address

			// 3D slider. Float in range 0.0 = off, 1.0 = max.
			case ConfigMem::SliderState3D: return Helpers::bit_cast<u32, float>(0.0f);
			case ConfigMem::FirmUnknown:
				return u32(read8(vaddr)) | (u32(read8(vaddr + 1)) << 8) | (u32(read8(vaddr + 2)) << 16) | (u32(read8(vaddr + 3)) << 24);

			default:
				if (vaddr >= VirtualAddrs::VramStart && vaddr < VirtualAddrs::VramStart + VirtualAddrs::VramSize) {
					static int shutUpCounter = 0;
					if (shutUpCounter < 5) { // Stop spamming about VRAM reads after the first 5
						shutUpCounter++;
						Helpers::warn("VRAM read!\n");
					}

					// TODO: Properly handle framebuffer readbacks and the like
					return *(u32*)&vram[vaddr - VirtualAddrs::VramStart];
				}

				Helpers::panic("Unimplemented 32-bit read, addr: %08X", vaddr);
				break;
		}
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
	} else {
		// VRAM write
		if (vaddr >= VirtualAddrs::VramStart && vaddr < VirtualAddrs::VramStart + VirtualAddrs::VramSize) {
			// TODO: Invalidate renderer caches here
			vram[vaddr - VirtualAddrs::VramStart] = value;
		}

		else {
			Helpers::panic("Unimplemented 8-bit write, addr: %08X, val: %02X", vaddr, value);
		}
	}
}

void Memory::write16(u32 vaddr, u16 value) {
	const u32 page = vaddr >> pageShift;
	const u32 offset = vaddr & pageMask;

	uintptr_t pointer = writeTable[page];
	if (pointer != 0) [[likely]] {
		*(u16*)(pointer + offset) = value;
	} else {
		Helpers::panic("Unimplemented 16-bit write, addr: %08X, val: %08X", vaddr, value);
	}
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
		if (c == '\0') {
			break;
		}

		string.push_back(c);
	}
	string.shrink_to_fit();

	return string;
}

// Return a pointer to the linear heap vaddr based on the kernel ver, because it needed to be moved
// thanks to the New 3DS having more FCRAM
u32 Memory::getLinearHeapVaddr() { return (kernelVersion < 0x22C) ? VirtualAddrs::LinearHeapStartOld : VirtualAddrs::LinearHeapStartNew; }

std::optional<u32> Memory::allocateMemory(u32 vaddr, u32 paddr, u32 size, bool linear, bool r, bool w, bool x, bool adjustAddrs, bool isMap) {
	// Kernel-allocated memory & size must always be aligned to a page boundary
	// Additionally assert we don't OoM and that we don't try to allocate physical FCRAM past what's available to userland
	// If we're mapping there's no fear of OoM, because we're not really allocating memory, just binding vaddrs to specific paddrs
	assert(isAligned(vaddr) && isAligned(paddr) && isAligned(size));
	assert(size <= FCRAM_APPLICATION_SIZE || isMap);
	assert(usedUserMemory + size <= FCRAM_APPLICATION_SIZE || isMap);
	assert(paddr + size <= FCRAM_APPLICATION_SIZE || isMap);

	// Amount of available user FCRAM pages and FCRAM pages to allocate respectively
	const u32 availablePageCount = (FCRAM_APPLICATION_SIZE - usedUserMemory) / pageSize;
	const u32 neededPageCount = size / pageSize;

	assert(availablePageCount >= neededPageCount || isMap);

	// If the paddr is 0, that means we need to select our own
	// TODO: Fix. This method always tries to allocate blocks linearly.
	// However, if the allocation is non-linear, the panic will trigger when it shouldn't.
	// Non-linear allocation needs special handling
	if (paddr == 0 && adjustAddrs) {
		std::optional<u32> newPaddr = findPaddr(size);
		if (!newPaddr.has_value()) {
			Helpers::panic("Failed to find paddr");
		}

		paddr = newPaddr.value();
		assert(paddr + size <= FCRAM_APPLICATION_SIZE || isMap);
	}

	// If the vaddr is 0 that means we need to select our own
	// Depending on whether our mapping should be linear or not we allocate from one of the 2 typical heap spaces
	// We don't plan on implementing freeing any time soon, so we can pick added userUserMemory to the vaddr base to
	// Get the full vaddr.
	// TODO: Fix this
	if (vaddr == 0 && adjustAddrs) {
		// Linear memory needs to be allocated in a way where you can easily get the paddr by subtracting the linear heap base
		// In order to be able to easily send data to hardware like the GPU
		if (linear) {
			vaddr = getLinearHeapVaddr() + paddr;
		} else {
			vaddr = usedUserMemory + VirtualAddrs::NormalHeapStart;
		}
	}

	if (!isMap) {
		usedUserMemory += size;
	}

	// Do linear mapping
	u32 virtualPage = vaddr >> pageShift;
	u32 physPage = paddr >> pageShift;  // TODO: Special handle when non-linear mapping is necessary
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
	assert(isAligned(size));
	const u32 neededPages = size / pageSize;

	// The FCRAM page we're testing to see if it's appropriate to use
	u32 candidatePage = 0;
	// The number of linear available pages we could find starting from this candidate page.
	// If this ends up >= than neededPages then the paddr is good (ie we can use the candidate page as a base address)
	u32 counter = 0;

	for (u32 i = 0; i < FCRAM_APPLICATION_PAGE_COUNT; i++) {
		if (usedFCRAMPages[i]) {  // Page is occupied already, go to new candidate
			candidatePage = i + 1;
			counter = 0;
		} else {  // The paddr we're testing has 1 more free page
			counter++;
			// Check if there's enough free memory to use this page
			// We use == instead of >= because some software does 0-byte allocations
			if (counter >= neededPages) {
				return candidatePage * pageSize;
			}
		}
	}

	// Couldn't find any page :(
	return std::nullopt;
}

u32 Memory::allocateSysMemory(u32 size) {
	// Should never be triggered, only here as a sanity check
	if (!isAligned(size)) {
		Helpers::panic("Memory::allocateSysMemory: Size is not page aligned (val = %08X)", size);
	}

	// We use a pretty dumb allocator for OS memory since this is not really accessible to the app and is only used internally
	// It works by just allocating memory linearly, starting from index 0 of OS memory and going up
	// This should also be unreachable in practice and exists as a sanity check
	if (size > remainingSysFCRAM()) {
		Helpers::panic("Memory::allocateSysMemory: Overflowed OS FCRAM");
	}

	const u32 pageCount = size / pageSize;                      // Number of pages that will be used up
	const u32 startIndex = sysFCRAMIndex() + usedSystemMemory;  // Starting FCRAM index
	const u32 startingPage = startIndex / pageSize;

	for (u32 i = 0; i < pageCount; i++) {
		if (usedFCRAMPages[startingPage + i])  // Also a theoretically unreachable panic for safety
			Helpers::panic("Memory::reserveMemory: Trying to reserve already reserved memory");
		usedFCRAMPages[startingPage + i] = true;
	}

	usedSystemMemory += size;
	return startIndex;
}

// The way I understand how the kernel's QueryMemory is supposed to work is that you give it a vaddr
// And the kernel looks up the memory allocations it's performed, finds which one it belongs in and returns its info?
// TODO: Verify this
MemoryInfo Memory::queryMemory(u32 vaddr) {
	// Check each allocation
	for (auto& alloc : memoryInfo) {
		// Check if the memory address belongs in this allocation and return the info if so
		if (vaddr >= alloc.baseAddr && vaddr < alloc.end()) {
			return alloc;
		}
	}

	// Otherwise, if this vaddr was never allocated
	// TODO: I think this is meant to return how much memory starting here is free as the size?
	return MemoryInfo(vaddr, pageSize, 0, KernelMemoryTypes::Free);
}

u8* Memory::mapSharedMemory(Handle handle, u32 vaddr, u32 myPerms, u32 otherPerms) {
	for (auto& e : sharedMemBlocks) {
		if (e.handle == handle) {
			// Virtual Console titles trigger this. TODO: Investigate how it should work
			if (e.mapped) Helpers::warn("Allocated shared memory block twice. Is this allowed?");

			const u32 paddr = e.paddr;
			const u32 size = e.size;

			if (myPerms == 0x10000000) {
				myPerms = 3;
				Helpers::panic("Memory::mapSharedMemory with DONTCARE perms");
			}

			bool r = myPerms & 0b001;
			bool w = myPerms & 0b010;
			bool x = myPerms & 0b100;

			const auto result = allocateMemory(vaddr, paddr, size, true, r, w, x, false, true);
			e.mapped = true;
			if (!result.has_value()) {
				Helpers::panic("Memory::mapSharedMemory: Failed to map shared memory block");
				return nullptr;
			}

			return &fcram[paddr];
		}
	}

	// This should be unreachable but better safe than sorry
	Helpers::panic("Memory::mapSharedMemory: Unknown shared memory handle %08X", handle);
	return nullptr;
}

void Memory::mirrorMapping(u32 destAddress, u32 sourceAddress, u32 size) {
	// Should theoretically be unreachable, only here for safety purposes
	assert(isAligned(destAddress) && isAligned(sourceAddress) && isAligned(size));

	const u32 pageCount = size / pageSize;  // How many pages we need to mirror
	for (u32 i = 0; i < pageCount; i++) {
		// Redo the shift here to "properly" handle wrapping around the address space instead of reading OoB
		const u32 sourcePage = sourceAddress / pageSize;
		const u32 destPage = destAddress / pageSize;

		readTable[destPage] = readTable[sourcePage];
		writeTable[destPage] = writeTable[sourcePage];

		sourceAddress += pageSize;
		destAddress += pageSize;
	}
}

// Get the number of ms since Jan 1 1900
u64 Memory::timeSince3DSEpoch() {
	using namespace std::chrono;

	std::time_t rawTime = std::time(nullptr);   // Get current UTC time
	auto localTime = std::localtime(&rawTime);  // Convert to local time

	bool daylightSavings = localTime->tm_isdst > 0;  // Get if time includes DST
	localTime = std::gmtime(&rawTime);

	// Use gmtime + mktime to calculate difference between local time and UTC
	auto timezoneDifference = rawTime - std::mktime(localTime);
	if (daylightSavings) {
		timezoneDifference += 60ull * 60ull;  // Add 1 hour (60 seconds * 60 minutes)
	}

	// seconds between Jan 1 1900 and Jan 1 1970
	constexpr u64 offset = 2208988800ull;
	milliseconds ms = duration_cast<milliseconds>(seconds(rawTime + timezoneDifference + offset));
	return ms.count();
}

Regions Memory::getConsoleRegion() {
	// TODO: Let the user force the console region as they want
	// For now we pick one based on the ROM header
	return region;
}

void Memory::copySharedFont(u8* pointer) {
	auto fonts = cmrc::ConsoleFonts::get_filesystem();
	auto font = fonts.open("CitraSharedFontUSRelocated.bin");
	std::memcpy(pointer, font.begin(), font.size());
}