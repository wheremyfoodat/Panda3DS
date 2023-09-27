#pragma once
#include "result_common.hpp"

DEFINE_HORIZON_RESULT_MODULE(Result::Kernel, Kernel);

namespace Result::Kernel {
	// Returned when a thread releases a mutex it does not own
	DEFINE_HORIZON_RESULT(InvalidMutexRelease, 31, InvalidArgument, Permanent);
	DEFINE_HORIZON_RESULT(NotFound, 1018, NotFound, Permanent);
	DEFINE_HORIZON_RESULT(InvalidEnumValue, 1005, InvalidArgument, Permanent);
	DEFINE_HORIZON_RESULT(InvalidHandle, 1015, InvalidArgument, Permanent);

	static_assert(InvalidHandle == 0xD8E007F7, "conversion broken");
};  // namespace Result::Kernel
