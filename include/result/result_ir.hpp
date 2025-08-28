#pragma once
#include "result_common.hpp"

DEFINE_HORIZON_RESULT_MODULE(Result::IR, IR);

namespace Result::IR {
	DEFINE_HORIZON_RESULT(NoDeviceConnected, 13, InvalidState, Status);
};
