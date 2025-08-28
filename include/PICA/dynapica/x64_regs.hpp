#pragma once

#ifdef PANDA3DS_X64_HOST
#include "xbyak/xbyak.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#define PANDA3DS_MS_ABI
constexpr Xbyak::Reg32 arg1 = Xbyak::util::ecx;  // register where first arg is stored
constexpr Xbyak::Reg32 arg2 = Xbyak::util::edx;  // register where second arg is stored
constexpr Xbyak::Reg32 arg3 = Xbyak::util::r8d;  // register where third arg is stored
constexpr Xbyak::Reg32 arg4 = Xbyak::util::r9d;  // register where fourth arg is stored

// Similar for floating point and vector arguemnts.
constexpr Xbyak::Xmm arg1f = Xbyak::util::xmm0;
constexpr Xbyak::Xmm arg2f = Xbyak::util::xmm1;
constexpr Xbyak::Xmm arg3f = Xbyak::util::xmm2;
constexpr Xbyak::Xmm arg4f = Xbyak::util::xmm3;

constexpr bool isWindows() { return true; }

#else  // System V calling convention
#define PANDA3DS_SYSV_ABI
constexpr Xbyak::Reg32 arg1 = Xbyak::util::edi;
constexpr Xbyak::Reg32 arg2 = Xbyak::util::esi;
constexpr Xbyak::Reg32 arg3 = Xbyak::util::edx;
constexpr Xbyak::Reg32 arg4 = Xbyak::util::ecx;

constexpr Xbyak::Xmm arg1f = Xbyak::util::xmm0;
constexpr Xbyak::Xmm arg2f = Xbyak::util::xmm1;
constexpr Xbyak::Xmm arg3f = Xbyak::util::xmm2;
constexpr Xbyak::Xmm arg4f = Xbyak::util::xmm3;
constexpr Xbyak::Xmm arg5f = Xbyak::util::xmm4;
constexpr Xbyak::Xmm arg6f = Xbyak::util::xmm5;
constexpr Xbyak::Xmm arg7f = Xbyak::util::xmm6;
constexpr Xbyak::Xmm arg8f = Xbyak::util::xmm7;

constexpr bool isWindows() { return false; }
#endif
#endif  // PANDA3DS_X64_HOST