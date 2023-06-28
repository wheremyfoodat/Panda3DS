#pragma once

#ifdef PANDA3DS_X64_HOST
#include "xbyak/xbyak.h"
using namespace Xbyak;
using namespace Xbyak::util;

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#define PANDA3DS_MS_ABI
constexpr Reg32 arg1 = ecx;  // register where first arg is stored
constexpr Reg32 arg2 = edx;  // register where second arg is stored
constexpr Reg32 arg3 = r8d;  // register where third arg is stored
constexpr Reg32 arg4 = r9d;  // register where fourth arg is stored

// Similar for floating point and vector arguemnts.
constexpr Xmm arg1f = xmm0;
constexpr Xmm arg2f = xmm1;
constexpr Xmm arg3f = xmm2;
constexpr Xmm arg4f = xmm3;

constexpr bool isWindows() { return true; }

#else  // System V calling convention
#define PANDA3DS_SYSV_ABI
constexpr Reg32 arg1 = edi;
constexpr Reg32 arg2 = esi;
constexpr Reg32 arg3 = edx;
constexpr Reg32 arg4 = ecx;

constexpr Xmm arg1f = xmm0;
constexpr Xmm arg2f = xmm1;
constexpr Xmm arg3f = xmm2;
constexpr Xmm arg4f = xmm3;
constexpr Xmm arg5f = xmm4;
constexpr Xmm arg6f = xmm5;
constexpr Xmm arg7f = xmm6;
constexpr Xmm arg8f = xmm7;

constexpr bool isWindows() { return false; }
#endif
#endif  // PANDA3DS_X64_HOST