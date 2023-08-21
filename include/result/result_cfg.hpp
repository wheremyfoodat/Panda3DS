#pragma once
#include "result_common.hpp"

DEFINE_HORIZON_RESULT_MODULE(Result::CFG, Config);

namespace Result::CFG {
	DEFINE_HORIZON_RESULT(NotFound, 1018, WrongArgument, Permanent);
};
