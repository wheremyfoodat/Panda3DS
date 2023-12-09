#ifndef DUCKSTATION_COMPAT_H
#define DUCKSTATION_COMPAT_H

#include <assert.h>

#include "compiler_builtins.hpp"
#include "helpers.hpp"

#define AssertMsg(cond, msg) assert(cond&& msg)
#define Assert(cond) assert(cond)

#define Panic(msg) assert(false && msg)

#define UnreachableCode() __builtin_unreachable()

#endif