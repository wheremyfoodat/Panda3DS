#include "loader/3dsx.hpp"

#include <cstring>
#include <optional>
#include <span>

#include "memory.hpp"

namespace {
	struct LoadInfo {
		u32 codeSegSizeAligned;
		u32 rodataSegSizeAligned;
		u32 dataSegSizeAligned;
	};

	static inline u32 translateAddr(const u32 off, const u32* addrs, const u32* offsets) {
		if (off < offsets[1]) {
			return addrs[0] + off;
		}

		if (off < offsets[2]) {
			return addrs[1] + off - offsets[1];
		}
		return addrs[2] + off - offsets[2];
	}
}  // namespace

bool Memory::map3DSX(HB3DSX& hb3dsx, const HB3DSX::Header& header) {
	const LoadInfo hbInfo = {
		.codeSegSizeAligned = (header.codeSegSize + 0xFFF) & ~0xFFF,
		.rodataSegSizeAligned = (header.rodataSegSize + 0xFFF) & ~0xFFF,
		.dataSegSizeAligned = (header.dataSegSize + 0xFFF) & ~0xFFF,
	};

	const u32 textSegAddr = HB3DSX::entrypoint;
	const u32 rodataSegAddr = textSegAddr + hbInfo.codeSegSizeAligned;
	const u32 dataSegAddr = rodataSegAddr + hbInfo.rodataSegSizeAligned;
	const u32 extraPageAddr = dataSegAddr + hbInfo.dataSegSizeAligned;

	printf("Text address = %08X, size = %08X\n", textSegAddr, hbInfo.codeSegSizeAligned);
	printf("Rodata address = %08X, size = %08X\n", rodataSegAddr, hbInfo.rodataSegSizeAligned);
	printf("Data address = %08X, size = %08X\n", dataSegAddr, hbInfo.dataSegSizeAligned);

	// Allocate stack, 3dsx/libctru don't require anymore than this
	if (!allocateMainThreadStack(4_KB)) {
		// Should be unreachable
		printf("Failed to allocate stack for 3DSX.\n");
		return false;
	}

	// Map code file to memory
	// Total memory to allocate for loading
	// suum of aligned values is always aligned, have an extra RW page for libctru
	const u32 totalSize = hbInfo.codeSegSizeAligned + hbInfo.rodataSegSizeAligned + hbInfo.dataSegSizeAligned + 4_KB;

	const auto opt = findPaddr(totalSize);
	if (!opt.has_value()) {
		Helpers::panic("Failed to find paddr to map 3DSX file's code to");
		return false;
	}

	// Map the ROM on the kernel side
	const u32 textOffset = 0;
	const u32 rodataOffset = textOffset + hbInfo.codeSegSizeAligned;
	const u32 dataOffset = rodataOffset + hbInfo.rodataSegSizeAligned;
	const u32 extraPageOffset = dataOffset + hbInfo.dataSegSizeAligned;

	std::array<HB3DSX::RelocHeader, 3> relocHeaders;
	auto [success, count] = hb3dsx.file.read(&relocHeaders[0], relocHeaders.size(), sizeof(HB3DSX::RelocHeader));
	if (!success || count != relocHeaders.size()) {
		Helpers::panic("Failed to read 3DSX relocation headers");
		return false;
	}

	const u32 dataLoadsize = header.dataSegSize - header.bssSize;  // 3DSX data size in header includes bss
	std::vector<u8> code(totalSize, 0);

	std::tie(success, count) = hb3dsx.file.readBytes(&code[textOffset], header.codeSegSize);
	if (!success || count != header.codeSegSize) {
		Helpers::panic("Failed to read 3DSX text segment");
		return false;
	}

	std::tie(success, count) = hb3dsx.file.readBytes(&code[rodataOffset], header.rodataSegSize);
	if (!success || count != header.rodataSegSize) {
		Helpers::panic("Failed to read 3DSX rodata segment");
		return false;
	}

	std::tie(success, count) = hb3dsx.file.readBytes(&code[dataOffset], dataLoadsize);
	if (!success || count != dataLoadsize) {
		Helpers::panic("Failed to read 3DSX data segment");
		return false;
	}

	std::vector<HB3DSX::Reloc> currentRelocs;

	const u32 segAddrs[] = {
		textSegAddr,
		rodataSegAddr,
		dataSegAddr,
		extraPageAddr,
	};

	const u32 segOffs[] = {
		textOffset,
		rodataOffset,
		dataOffset,
		extraPageOffset,
	};

	const u32 segSizes[] = {
		header.codeSegSize,
		header.rodataSegSize,
		dataLoadsize,
		0x1000,
	};

	for (const auto& relocHeader : relocHeaders) {
		currentRelocs.resize(relocHeader.absoluteCount + relocHeader.relativeCount);
		std::tie(success, count) = hb3dsx.file.read(&currentRelocs[0], currentRelocs.size(), sizeof(HB3DSX::Reloc));
		if (!success || count != currentRelocs.size()) {
			Helpers::panic("Failed to read 3DSX relocations");
			return false;
		}

		const auto allRelocs = std::span(currentRelocs);
		const auto absoluteRelocs = allRelocs.subspan(0, relocHeader.absoluteCount);
		const auto relativeRelocs = allRelocs.subspan(relocHeader.absoluteCount, relocHeader.relativeCount);

		const auto currentSeg = &relocHeader - &relocHeaders[0];
		const auto sectionDataStartAs = std::span(code).subspan(segOffs[currentSeg], segSizes[currentSeg]);
		auto sectionData = sectionDataStartAs;

		const auto RelocationAction = [&](const HB3DSX::Reloc& reloc, const HB3DSX::RelocType relocType) -> bool {
			if (reloc.skip) {
				sectionData = sectionData.subspan(reloc.skip * sizeof(u32));  // advance by `skip` words (32-bit values)
			}

			for (u32 m = 0; m < reloc.patch && !sectionData.empty(); ++m) {
				const u32 inAddr = textSegAddr + (sectionData.data() - code.data());  // byte offset -> word count
				u32 origData = 0;
				std::memcpy(&origData, &sectionData[0], sizeof(u32));
				const u32 subType = origData >> (32 - 4);
				const u32 addr = translateAddr(origData & ~0xF0000000, segAddrs, segOffs);

				switch (relocType) {
					case HB3DSX::RelocType::Absolute: {
						if (subType != 0) {
							Helpers::panic("Unsupported absolute reloc subtype");
							return false;
						}
						std::memcpy(&sectionData[0], &addr, sizeof(u32));
						break;
					}

					case HB3DSX::RelocType::Relative: {
						u32 data = addr - inAddr;
						switch (subType) {
							case 1:  // 31-bit signed offset
								data &= ~(1u << 31);
							case 0:  // 32-bit signed offset
								std::memcpy(&sectionData[0], &data, sizeof(u32));
								break;
							default: Helpers::panic("Unsupported relative reloc subtype"); return false;
						}
						break;
					}
				}

				sectionData = sectionData.subspan(sizeof(u32));
			}

			return true;
		};

		for (const auto& reloc : absoluteRelocs) {
			if (!RelocationAction(reloc, HB3DSX::RelocType::Absolute)) {
				return false;
			}
		}

		sectionData = sectionDataStartAs;  // restart from the beginning for the next part
		for (const auto& reloc : relativeRelocs) {
			if (!RelocationAction(reloc, HB3DSX::RelocType::Relative)) {
				return false;
			}
		}
	}

	// Detect and fill _prm structure
	HB3DSX::PrmStruct pst;
	std::memcpy(&pst, &code[4], sizeof(pst));
	if (pst.magic[0] == '_' && pst.magic[1] == 'p' && pst.magic[2] == 'r' && pst.magic[3] == 'm') {
		// if there was any argv to put, it would go there
		// first u32: argc
		// remaining: continuous argv string (NUL-char separated, ofc)
		// std::memcpy(&code[extraPageOffset], argvBuffer, ...);

		// setting to NULL (default) = run from system. load romfs from process.
		// non-NULL = homebrew launcher. load romfs from 3dsx @ argv[0]
		// pst.pSrvOverride = extraPageAddr + 0xFFC;

		pst.pArgList = extraPageAddr;

		// RUNFLAG_APTREINIT: Reinitialize APT.
		// From libctru. Because there's no previously running software here
		pst.runFlags |= 1 << 1;

		/* s64 dummy;
		bool isN3DS = svcGetSystemInfo(&dummy, 0x10001, 0) == 0;
		if (isN3DS)
		{
			pst->heapSize = u32(48_MB);
			pst->linearHeapSize = u32(64_MB);
		} else */ {
			pst.heapSize = u32(24_MB);
			pst.linearHeapSize = u32(32_MB);
		}

		std::memcpy(&code[4], &pst, sizeof(pst));
	}

	const auto paddr = opt.value();
	std::memcpy(&fcram[paddr], &code[0], totalSize);  // Copy the 3 segments + BSS to FCRAM

	allocateMemory(textSegAddr, paddr + textOffset, hbInfo.codeSegSizeAligned, true, true, false, true);           // Text is R-X
	allocateMemory(rodataSegAddr, paddr + rodataOffset, hbInfo.rodataSegSizeAligned, true, true, false, false);    // Rodata is R--
	allocateMemory(dataSegAddr, paddr + dataOffset, hbInfo.dataSegSizeAligned + 0x1000, true, true, true, false);  // Data+BSS+Extra is RW-

	return true;
}

std::optional<u32> Memory::load3DSX(const std::filesystem::path& path) {
	HB3DSX hb3dsx;
	if (!hb3dsx.file.open(path, "rb")) {
		return std::nullopt;
	}

	u8 magic[4];  // Must be "3DSX"
	auto [success, bytes] = hb3dsx.file.readBytes(magic, 4);

	if (!success || bytes != 4) {
		printf("Failed to read 3DSX magic\n");
		return std::nullopt;
	}

	if (magic[0] != '3' || magic[1] != 'D' || magic[2] != 'S' || magic[3] != 'X') {
		printf("3DSX with wrong magic value\n");
		return std::nullopt;
	}

	HB3DSX::Header hbHeader;
	std::tie(success, bytes) = hb3dsx.file.readBytes(&hbHeader, sizeof(hbHeader));
	if (!success || bytes != sizeof(hbHeader)) {
		printf("Failed to read 3DSX header\n");
		return std::nullopt;
	}

	if (hbHeader.headerSize == 0x20 || hbHeader.headerSize == 0x2C) {
		if (hbHeader.headerSize == 0x2C) {
			hb3dsx.file.seek(8, SEEK_CUR);  // skip SMDH info
			std::tie(success, bytes) = hb3dsx.file.readBytes(&hb3dsx.romFSOffset, 4);
			if (!success || bytes != 4) {
				printf("Failed to read 3DSX romFS offset\n");
				return std::nullopt;
			}

			const auto fileSize = hb3dsx.file.size();
			if (!fileSize) {
				printf("Failed to get 3DSX size\n");
				return std::nullopt;
			}
			hb3dsx.romFSSize = *fileSize - hb3dsx.romFSOffset;
		}
	} else {
		printf("Invalid 3DSX header size\n");
		return std::nullopt;
	}

	if (!map3DSX(hb3dsx, hbHeader)) {
		printf("Failed to map 3DSX\n");
		return std::nullopt;
	}

	loaded3DSX = std::move(hb3dsx);
	return HB3DSX::entrypoint;
}

bool HB3DSX::hasRomFs() const { return romFSSize != 0 && romFSOffset != 0; }

std::pair<bool, std::size_t> HB3DSX::readRomFSBytes(void* dst, std::size_t offset, std::size_t size) {
	if (!hasRomFs()) {
		return {false, 0};
	}

	if (!file.seek(romFSOffset + offset)) {
		return {false, 0};
	}

	return file.readBytes(dst, size);
}
