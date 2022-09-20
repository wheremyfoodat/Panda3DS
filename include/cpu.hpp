#pragma once

#ifdef CPU_DYNARMIC
#include "cpu_dynarmic.hpp"
#elif defined(CPU_KVM)
#error KVM CPU is not implemented yet
#else
#error No CPU core implemented :(
#endif