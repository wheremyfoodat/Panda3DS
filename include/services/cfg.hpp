#pragma once
#include <cstring>
#include "types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "region_codes.hpp"

class CFGService {
	Handle handle = KernelHandles::CFG;
	Memory& mem;
	CountryCodes country = CountryCodes::US; // Default to USA
	MAKE_LOG_FUNCTION(log, cfgLogger)

	void writeStringU16(u32 pointer, const std::u16string& string);

	// Service functions
	void getConfigInfoBlk2(u32 messagePointer);
	void getRegionCanadaUSA(u32 messagePointer);
	void getSystemModel(u32 messagePointer);
	void genUniqueConsoleHash(u32 messagePointer);
	void secureInfoGetRegion(u32 messagePointer);

public:
	CFGService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};
