#pragma once
#include <array>
#include <bitset>
#include <filesystem>
#include <fstream>
#include <optional>
#include <list>
#include <vector>

#include "config.hpp"
#include "crypto/aes_engine.hpp"
#include "handles.hpp"
#include "helpers.hpp"
#include "kernel/kernel_types.hpp"
#include "loader/ncsd.hpp"
#include "loader/3dsx.hpp"
#include "result/result.hpp"
#include "services/region_codes.hpp"

namespace PhysicalAddrs {
	enum : u32 {
		VRAM = 0x18000000,
		VRAMEnd = VRAM + 0x005FFFFF,
		FCRAM = 0x20000000,
		FCRAMEnd = FCRAM + 0x07FFFFFF,

		DSP_RAM = 0x1FF00000,
		DSP_RAM_End = DSP_RAM + 0x0007FFFF
	};
}

namespace VirtualAddrs {
	enum : u32 {
		ExecutableStart = 0x00100000,
		MaxExeSize = 0x03F00000,
		ExecutableEnd = 0x00100000 + 0x03F00000,

		// Stack for main ARM11 thread.
		// Typically 0x4000 bytes, determined by exheader
		StackTop = 0x10000000,
		DefaultStackSize = 0x4000,

		NormalHeapStart = 0x08000000,
		LinearHeapStartOld = 0x14000000, // If kernel version < 0x22C
		LinearHeapEndOld = 0x1C000000,

		LinearHeapStartNew = 0x30000000,
		LinearHeapEndNew = 0x40000000,

		// Start of TLS for first thread. Next thread's storage will be at TLSBase + 0x200, and so on
		TLSBase = 0x1FF82000,
		TLSSize = 0x200,

		VramStart = 0x1F000000,
		VramSize = 0x00600000,
		FcramTotalSize = 128_MB,
		DSPMemStart = 0x1FF00000
	};
}

// Types for svcQueryMemory
namespace KernelMemoryTypes {
	// This makes no sense
	enum MemoryState : u32 {
		Free = 0,
		Reserved = 1,
		IO = 2,
		Static = 3,
		Code = 4,
		Private = 5,
		Shared = 6,
		Continuous = 7,
		Aliased = 8,
		Alias = 9,
		AliasCode = 10,
		Locked = 11,

		PERMISSION_R = 1 << 0,
		PERMISSION_W = 1 << 1,
		PERMISSION_X = 1 << 2
	};
	
	struct MemoryInfo {
		u32 baseAddr;
		u32 pages;
		u32 perms;
		u32 state;

		u32 end() { return baseAddr + (pages << 12); }
		MemoryInfo() : baseAddr(0), pages(0), perms(0), state(0) {}
		MemoryInfo(u32 baseAddr, u32 pages, u32 perms, u32 state) : baseAddr(baseAddr), pages(pages), perms(perms), state(state) {}
	};

	// Shared memory block for HID, GSP:GPU etc
	struct SharedMemoryBlock {
		u32 paddr; // Physical address of this block's memory
		u32 size; // Size of block
		u32 handle; // The handle of the shared memory block
		bool mapped; // Has this block been mapped at least once?

		SharedMemoryBlock(u32 paddr, u32 size, u32 handle) : paddr(paddr), size(size), handle(handle), mapped(false) {}
	};
}

struct FcramBlock;
class KFcram;
enum class FcramRegion;

class Memory {
	// Used internally by changeMemoryState
	struct Operation {
		KernelMemoryTypes::MemoryState newState = KernelMemoryTypes::MemoryState::Free;
		bool r = false, w = false, x = false;
		bool changeState = false;
		bool changePerms = false;
	};
	using Handle = HorizonHandle;

	u8* fcram;
	u8* dspRam;  // Provided to us by Audio
	u8* vram;    // Provided to the memory class by the GPU class

	u64& cpuTicks; // Reference to the CPU tick counter
	using SharedMemoryBlock = KernelMemoryTypes::SharedMemoryBlock;

	// TODO: remove this reference when Peach's excellent page table code is moved to a better home
	KFcram& fcramManager;

	// Our dynarmic core uses page tables for reads and writes with 4096 byte pages
	std::vector<uintptr_t> readTable, writeTable;

	// vaddr->paddr translation table
	std::vector<u32> paddrTable;

	// This tracks our OS' memory allocations
	std::list<KernelMemoryTypes::MemoryInfo> memoryInfo;

	std::array<SharedMemoryBlock, 5> sharedMemBlocks = {
		SharedMemoryBlock(0, 0, KernelHandles::FontSharedMemHandle), // Shared memory for the system font (size is 0 because we read the size from the cmrc filesystem
		SharedMemoryBlock(0, 0x1000, KernelHandles::GSPSharedMemHandle), // GSP shared memory
		SharedMemoryBlock(0, 0x1000, KernelHandles::HIDSharedMemHandle),  // HID shared memory
		SharedMemoryBlock(0, 0x3000, KernelHandles::CSNDSharedMemHandle), // CSND shared memory
		SharedMemoryBlock(0, 0xE7000, KernelHandles::APTCaptureSharedMemHandle),  // APT Capture Buffer memory
 	};

public:
	static constexpr u32 pageShift = 12;
	static constexpr u32 pageSize = 1 << pageShift;
	static constexpr u32 pageMask = pageSize - 1;
	static constexpr u32 totalPageCount = 1 << (32 - pageShift);
	
	static constexpr u32 FCRAM_SIZE = u32(128_MB);
	static constexpr u32 FCRAM_APPLICATION_SIZE = u32(64_MB);
	static constexpr u32 FCRAM_SYSTEM_SIZE = u32(44_MB);
	static constexpr u32 FCRAM_BASE_SIZE = u32(20_MB);

	static constexpr u32 FCRAM_PAGE_COUNT = FCRAM_SIZE / pageSize;
	static constexpr u32 FCRAM_APPLICATION_PAGE_COUNT = FCRAM_APPLICATION_SIZE / pageSize;

	static constexpr u32 DSP_RAM_SIZE = u32(512_KB);
	static constexpr u32 DSP_CODE_MEMORY_OFFSET = u32(0_KB);
	static constexpr u32 DSP_DATA_MEMORY_OFFSET = u32(256_KB);

private:
	//std::bitset<FCRAM_PAGE_COUNT> usedFCRAMPages;
	//std::optional<u32> findPaddr(u32 size);
	u64 timeSince3DSEpoch();

	// https://www.3dbrew.org/wiki/Configuration_Memory#ENVINFO
	// Report a retail unit without JTAG
	static constexpr u32 envInfo = 1;

	// Stored in Configuration Memory starting @ 0x1FF80060 
	struct FirmwareInfo {
		u8 unk; // Usually 0 according to 3DBrew
		u8 revision;
		u8 minor;
		u8 major;
		u32 syscoreVer;
		u32 sdkVer;
	};

	// Values taken from 3DBrew and Citra
	static constexpr FirmwareInfo firm{.unk = 0, .revision = 0, .minor = 0x34, .major = 2, .syscoreVer = 2, .sdkVer = 0x0000F297};
	// Adjusted upon loading a ROM based on the ROM header. Used by CFG::SecureInfoGetArea to get past region locks
	Regions region = Regions::USA;
	const EmulatorConfig& config;

	static constexpr std::array<u8, 6> MACAddress = {0x40, 0xF4, 0x07, 0xFF, 0xFF, 0xEE};

	void changeMemoryState(u32 vaddr, s32 pages, const Operation& op);
	void queryPhysicalBlocks(std::list<FcramBlock>& outList, u32 vaddr, s32 pages);
	void mapPhysicalMemory(u32 vaddr, u32 paddr, s32 pages, bool r, bool w, bool x);
	void unmapPhysicalMemory(u32 vaddr, u32 paddr, s32 pages);

  public:
	u16 kernelVersion = 0;

	Memory(KFcram& fcramManager, u64& cpuTicks, const EmulatorConfig& config);
	void reset();
	void* getReadPointer(u32 address);
	void* getWritePointer(u32 address);
	std::optional<u32> loadELF(std::ifstream& file);
	std::optional<u32> load3DSX(const std::filesystem::path& path);
	std::optional<NCSD> loadNCSD(Crypto::AESEngine& aesEngine, const std::filesystem::path& path);
	std::optional<NCSD> loadCXI(Crypto::AESEngine& aesEngine, const std::filesystem::path& path);

	bool mapCXI(NCSD& ncsd, NCCH& cxi);
	bool map3DSX(HB3DSX& hb3dsx, const HB3DSX::Header& header);

	u8 read8(u32 vaddr);
	u16 read16(u32 vaddr);
	u32 read32(u32 vaddr);
	u64 read64(u32 vaddr);
	std::string readString(u32 vaddr, u32 maxCharacters);

	void write8(u32 vaddr, u8 value);
	void write16(u32 vaddr, u16 value);
	void write32(u32 vaddr, u32 value);
	void write64(u32 vaddr, u64 value);

	u32 getLinearHeapVaddr();
	u8* getFCRAM() { return fcram; }

	enum class BatteryLevel {
		Empty = 0,
		AlmostEmpty,
		OneBar,
		TwoBars,
		ThreeBars,
		FourBars,
	};

	u8 getBatteryState(bool adapterConnected, bool charging, BatteryLevel batteryLevel) {
		u8 value = static_cast<u8>(batteryLevel) << 2; // Bits 2:4 are the battery level from 0 to 5
		if (adapterConnected) value |= 1 << 0; // Bit 0 shows if the charger is connected
		if (charging) value |= 1 << 1; // Bit 1 shows if we're charging

		return value;
	}

	NCCH* getCXI() {
		if (loadedCXI.has_value()) {
			return &loadedCXI.value();
		} else {
			return nullptr;
		}
	}

	HB3DSX* get3DSX() {
		if (loaded3DSX.has_value()) {
			return &loaded3DSX.value();
		} else {
			return nullptr;
		}
	}

	// Returns whether "addr" is aligned to a page (4096 byte) boundary
	static constexpr bool isAligned(u32 addr) {
		return (addr & pageMask) == 0;
	}

	bool allocMemory(u32 vaddr, s32 pages, FcramRegion region, bool r, bool w, bool x, KernelMemoryTypes::MemoryState state);
	bool allocMemoryLinear(u32& outVaddr, u32 inVaddr, s32 pages, FcramRegion region, bool r, bool w, bool x);
	bool mapVirtualMemory(u32 dstVaddr, u32 srcVaddr, s32 pages, bool r, bool w, bool x,
		KernelMemoryTypes::MemoryState oldDstState, KernelMemoryTypes::MemoryState oldSrcState, 
		KernelMemoryTypes::MemoryState newDstState, KernelMemoryTypes::MemoryState newSrcState);
	void changePermissions(u32 vaddr, s32 pages, bool r, bool w, bool x);
	Result::HorizonResult queryMemory(KernelMemoryTypes::MemoryInfo& out, u32 vaddr);
	Result::HorizonResult testMemoryState(u32 vaddr, s32 pages, KernelMemoryTypes::MemoryState desiredState);

	void copyToVaddr(u32 dstVaddr, const u8* srcHost, s32 size);

	// Map a shared memory block to virtual address vaddr with permissions "myPerms"
	// The kernel has a second permission parameter in MapMemoryBlock but not sure what's used for
	// TODO: Find out
	// Returns a pointer to the FCRAM block used for the memory if allocation succeeded
	u8* mapSharedMemory(Handle handle, u32 vaddr, u32 myPerms, u32 otherPerms);

	// Backup of the game's CXI partition info, if any
	std::optional<NCCH> loadedCXI = std::nullopt;
	std::optional<HB3DSX> loaded3DSX = std::nullopt;
	// File handle for reading the loaded ncch
	IOFile CXIFile;

	std::optional<u64> getProgramID();

	u8* getDSPMem() { return dspRam; }
	u8* getDSPDataMem() { return &dspRam[DSP_DATA_MEMORY_OFFSET]; }
	u8* getDSPCodeMem() { return &dspRam[DSP_CODE_MEMORY_OFFSET]; }

	void setVRAM(u8* pointer) { vram = pointer; }
	void setDSPMem(u8* pointer) { dspRam = pointer; }

	bool allocateMainThreadStack(u32 size);
	Regions getConsoleRegion();
	void copySharedFont(u8* ptr, u32 vaddr);
};
