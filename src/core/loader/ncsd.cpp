#include "loader/ncsd.hpp"

#include <cstring>
#include <optional>

#include "memory.hpp"
#include "kernel/fcram.hpp"

using namespace KernelMemoryTypes;

bool Memory::mapCXI(NCSD& ncsd, NCCH& cxi) {
	printf("Text address = %08X, size = %08X\n", cxi.text.address, cxi.text.size);
	printf("Rodata address = %08X, size = %08X\n", cxi.rodata.address, cxi.rodata.size);
	printf("Data address = %08X, size = %08X\n", cxi.data.address, cxi.data.size);
	printf("Stack size: %08X\n", cxi.stackSize);

	static constexpr std::array<const char*, 7> regionNames = {"Japan", "North America", "Europe", "Australia", "China", "Korea", "Taiwan"};

	// Set autodetected 3DS region to one of the values allowed by the CXI's SMDH
	region = cxi.region.value();
	printf("Console region autodetected to: %s\n", regionNames[static_cast<size_t>(region)]);

	if (!isAligned(cxi.stackSize)) {
		Helpers::warn("CXI has a suspicious stack size of %08X which is not a multiple of 4KB", cxi.stackSize);
	}

	// Round up the size of the CXI stack size to a page (4KB) boundary, as the OS can only allocate memory this way
	u32 stackSize = (cxi.stackSize + pageSize - 1) & -pageSize;

	// Allocate stack
	if (!allocateMainThreadStack(stackSize)) {
		// Should be unreachable
		printf("Failed to allocate stack for CXI partition. Requested stack size: %08X\n", stackSize);
		return false;
	}

	// Map code file to memory
	auto& code = cxi.codeFile;
	u32 bssSize = (cxi.bssSize + 0xfff) & ~0xfff;  // Round BSS size up to a page boundary
	// Total memory to allocate for loading
	u32 totalSize = (cxi.text.pageCount + cxi.rodata.pageCount + cxi.data.pageCount) * pageSize + bssSize;
	code.resize(code.size() + bssSize, 0);  // Pad the .code file with zeroes for the BSS segment

	if (code.size() < totalSize) {
		Helpers::panic("Total code size as reported by the exheader is larger than the .code file");
		return false;
	}

	// Map the ROM on the kernel side
	u32 textOffset = 0;
	u32 textAddr = cxi.text.address;
	u32 textSize = cxi.text.pageCount * pageSize;

	u32 rodataOffset = textOffset + textSize;
	u32 rodataAddr = cxi.rodata.address;
	u32 rodataSize = cxi.rodata.pageCount * pageSize;

	u32 dataOffset = rodataOffset + rodataSize;
	u32 dataAddr = cxi.data.address;
	u32 dataSize = cxi.data.pageCount * pageSize + bssSize;  // We're merging the data and BSS segments, as BSS is just pre-initted .data

	// TODO: base this off the exheader
	auto region = FcramRegion::App;

	allocMemory(textAddr, cxi.text.pageCount, region, true, false, true, MemoryState::Code);
	allocMemory(rodataAddr, cxi.rodata.pageCount, region, true, false, false, MemoryState::Code);
	allocMemory(dataAddr, cxi.data.pageCount, region, true, true, false, MemoryState::Private);
	allocMemory(dataAddr + (cxi.data.pageCount << 12), bssSize >> 12, region, true, true, false, MemoryState::Private);

	// Copy .code file to FCRAM
	copyToVaddr(textAddr, code.data(), textSize);
	copyToVaddr(rodataAddr, code.data() + textSize, rodataSize);
	copyToVaddr(dataAddr, code.data() + textSize + rodataSize, cxi.data.pageCount << 12);

	ncsd.entrypoint = cxi.text.address;

	// Back the IOFile for accessing the ROM, as well as the ROM's CXI partition, in the memory class.
	CXIFile = ncsd.file;
	loadedCXI = cxi;
	return true;
}

std::optional<NCSD> Memory::loadNCSD(Crypto::AESEngine& aesEngine, const std::filesystem::path& path) {
	NCSD ncsd;
	if (!ncsd.file.open(path, "rb")) return std::nullopt;

	u8 magic[4];  // Must be "NCSD"
	ncsd.file.seek(0x100);
	auto [success, bytes] = ncsd.file.readBytes(magic, 4);

	if (!success || bytes != 4) {
		printf("Failed to read NCSD magic\n");
		return std::nullopt;
	}

	if (magic[0] != 'N' || magic[1] != 'C' || magic[2] != 'S' || magic[3] != 'D') {
		printf("NCSD with wrong magic value\n");
		return std::nullopt;
	}

	std::tie(success, bytes) = ncsd.file.readBytes(&ncsd.size, 4);
	if (!success || bytes != 4) {
		printf("Failed to read NCSD size\n");
		return std::nullopt;
	}

	ncsd.size *= NCSD::mediaUnit;  // Convert size to bytes

	// Read partition data
	ncsd.file.seek(0x120);
	// 2 u32s per partition (offset and length), 8 partitions total
	constexpr size_t partitionDataSize = 8 * 2;  // Size of partition in u32s
	u32 partitionData[8 * 2];
	std::tie(success, bytes) = ncsd.file.read(partitionData, partitionDataSize, sizeof(u32));
	if (!success || bytes != partitionDataSize) {
		printf("Failed to read NCSD partition data\n");
		return std::nullopt;
	}

	for (int i = 0; i < 8; i++) {
		auto& partition = ncsd.partitions[i];
		NCCH& ncch = partition.ncch;
		partition.offset = u64(partitionData[i * 2]) * NCSD::mediaUnit;
		partition.length = u64(partitionData[i * 2 + 1]) * NCSD::mediaUnit;

		ncch.partitionIndex = i;
		ncch.fileOffset = partition.offset;

		if (partition.length != 0) {  // Initialize the NCCH of each partition
			NCCH::FSInfo ncchFsInfo;

			ncchFsInfo.offset = partition.offset;
			ncchFsInfo.size = partition.length;

			if (!ncch.loadFromHeader(aesEngine, ncsd.file, ncchFsInfo)) {
				printf("Invalid NCCH partition\n");
				return std::nullopt;
			}
		}
	}

	auto& cxi = ncsd.partitions[0].ncch;
	if (!cxi.hasExtendedHeader() || !cxi.hasCode()) {
		printf("NCSD with an invalid CXI in partition 0?\n");
		return std::nullopt;
	}

	if (!mapCXI(ncsd, cxi)) {
		printf("Failed to map CXI\n");
		return std::nullopt;
	}

	return ncsd;
}

// We are lazy so we take CXI files, easily "convert" them to NCSD internally, then use our existing NCSD infrastructure
// This is easy because NCSD is just CXI + some more NCCH partitions, which we can make empty when converting to NCSD
std::optional<NCSD> Memory::loadCXI(Crypto::AESEngine& aesEngine, const std::filesystem::path& path) {
	NCSD ncsd;
	if (!ncsd.file.open(path, "rb")) {
		return std::nullopt;
	}

	// Make partitions 1 through 8 of the converted NCSD empty
	// Partition 0 (CXI partition of an NCSD) is the only one we care about
	for (int i = 1; i < 8; i++) {
		auto& partition = ncsd.partitions[i];
		NCCH& ncch = partition.ncch;
		partition.offset = 0ull;
		partition.length = 0ull;

		ncch.partitionIndex = i;
		ncch.fileOffset = partition.offset;
	}

	auto& cxiPartition = ncsd.partitions[0];
	auto& cxi = cxiPartition.ncch;

	std::optional<u64> size = ncsd.file.size();
	if (!size.has_value()) {
		return std::nullopt;
	}

	cxiPartition.offset = 0ull;
	cxiPartition.length = size.value();
	NCCH::FSInfo cxiInfo{.offset = cxiPartition.offset, .size = cxiPartition.length, .hashRegionSize = 0, .encryptionInfo = std::nullopt};

	if (!cxi.loadFromHeader(aesEngine, ncsd.file, cxiInfo)) {
		printf("Invalid CXI partition\n");
		return std::nullopt;
	}

	if (!cxi.hasExtendedHeader() || !cxi.hasCode()) {
		printf("CXI does not have exheader or code file?\n");
		return std::nullopt;
	}

	if (!mapCXI(ncsd, cxi)) {
		printf("Failed to map CXI\n");
		return std::nullopt;
	}

	return ncsd;
}
