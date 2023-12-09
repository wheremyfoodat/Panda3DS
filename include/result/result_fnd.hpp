#pragma once
#include "result_common.hpp"

DEFINE_HORIZON_RESULT_MODULE(Result::FND, FND);

namespace Result::FND {
	DEFINE_HORIZON_RESULT(InvalidEnumValue, 1005, InvalidArgument, Permanent);
};
