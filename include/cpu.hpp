#pragma once

#ifdef CPU_DYNARMIC
#include "cpu_dynarmic.hpp"
#elif defined(CPU_KVM)
#include "cpu_kvm.hpp"
#else
#error No CPU core implemented :(
#endif