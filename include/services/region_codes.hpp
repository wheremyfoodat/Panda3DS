#pragma once
#include "helpers.hpp"

// Used for CFG::SecureInfoGetRegion
enum class Regions : u32 {
	Japan = 0,
	USA = 1,
	Europe = 2,
	Australia = 3,
	China = 4,
	Korea = 5,
	Taiwan = 6
};

// Used for the language field in the NAND user data
enum class LanguageCodes : u32 {
	JP = 0,
	EN = 1,
	FR = 2,
	DE = 3,
	IT = 4,
	ES = 5,
	ZH = 6,
	KO = 7,
	NL = 8,
	PT = 9,
	RU = 10,
	TW = 11,

	Japanese = JP,
	English = EN,
	French = FR,
	German = DE,
	Italian = IT,
	Spanish = ES,
	Chinese = ZH,
	Korean = KO,
	Dutch = NL,
	Portuguese = PT,
	Russian = RU,
	Taiwanese = TW
};