#pragma once
#include <cstdint>

// Codes for every 3DS system model
// The 3-letter codes are codenames used by the ACT service, and can also be found on the hardware itself
// This info can be retrieved in a variety of ways, usually through CFG::GetSystemModel
namespace SystemModel {
	enum : std::uint32_t {
		Nintendo3DS = 0,
		Nintendo3DS_XL = 1,
		NewNintendo3DS = 2,
		Nintendo2DS = 3,
		NewNintendo3DS_XL = 4,
		NewNintendo2DS_XL = 5,

		CTR = Nintendo3DS,
		SPR = Nintendo3DS_XL,
		KTR = NewNintendo3DS,
		FTR = Nintendo2DS,
		RED = NewNintendo3DS_XL,
		JAN = NewNintendo2DS_XL,
	};
}