#pragma once
#include "result_common.hpp"

DEFINE_HORIZON_RESULT_MODULE(Result::OS, OS);

namespace Result::OS {
    DEFINE_HORIZON_RESULT(InvalidPortName, 20, WrongArgument, Permanent);
    DEFINE_HORIZON_RESULT(PortNameTooLong, 30, InvalidArgument, Usage);
    DEFINE_HORIZON_RESULT(InvalidHandle, 1015, WrongArgument, Permanent);
    DEFINE_HORIZON_RESULT(InvalidCombination, 1006, InvalidArgument, Usage);
    DEFINE_HORIZON_RESULT(MisalignedAddress, 1009, InvalidArgument, Usage);
    DEFINE_HORIZON_RESULT(MisalignedSize, 1010, InvalidArgument, Usage);\
    DEFINE_HORIZON_RESULT(InvalidAddress, 1013, InvalidArgument, Usage);
    DEFINE_HORIZON_RESULT(OutOfRange, 1021, InvalidArgument, Usage);
    DEFINE_HORIZON_RESULT(Timeout, 1022, StatusChanged, Info);
};
