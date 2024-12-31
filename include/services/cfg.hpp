#pragma once
#include <cstring>

#include "config.hpp"
#include "helpers.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "region_codes.hpp"
#include "result/result.hpp"

class CFGService {
	using Handle = HorizonHandle;

	Memory& mem;
	const EmulatorConfig& settings;

	CountryCodes country = CountryCodes::US;  // Default to USA
	MAKE_LOG_FUNCTION(log, cfgLogger)

	void writeStringU16(u32 pointer, const std::u16string& string);

	// Service functions
	void getConfigInfoBlk2(u32 messagePointer);
	void getConfigInfoBlk8(u32 messagePointer, u32 commandWord);
	void getCountryCodeID(u32 messagePointer);
	void getLocalFriendCodeSeed(u32 messagePointer);
	void getRegionCanadaUSA(u32 messagePointer);
	void getSystemModel(u32 messagePointer);
	void genUniqueConsoleHash(u32 messagePointer);
	void secureInfoGetByte101(u32 messagePointer);
	void secureInfoGetRegion(u32 messagePointer);
	void setConfigInfoBlk4(u32 messagePointer);
	void updateConfigNANDSavegame(u32 messagePointer);
	void translateCountryInfo(u32 messagePointer);
	void isFangateSupported(u32 messagePointer);

	// cfg:nor functions
	void norInitialize(u32 messagePointer);
	void norReadData(u32 messagePointer);

	void getConfigInfo(u32 output, u32 blockID, u32 size, u32 permissionMask);

  public:
	enum class Type {
		U,    // cfg:u
		I,    // cfg:i
		S,    // cfg:s
		NOR,  // cfg:nor
	};

	CFGService(Memory& mem, const EmulatorConfig& settings) : mem(mem), settings(settings) {}
	void reset();
	void handleSyncRequest(u32 messagePointer, Type type);
};