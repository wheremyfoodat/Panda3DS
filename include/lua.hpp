#pragma once
#include "helpers.hpp"
#include "memory.hpp"

#ifdef PANDA3DS_ENABLE_LUA
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include "luajit.h"
}

class LuaManager {
	lua_State* L = nullptr;
	bool initialized = false;

  public:
	// For Lua we must have some global pointers to our emulator objects to use them in script code via thunks. See the thunks in lua.cpp as an
	// example
	static Memory* g_memory;

	LuaManager(Memory& mem) { g_memory = &mem; }

	void close();
	void initialize();
	void initializeThunks();
	void loadFile(const char* path);
	void reset();
};

#elif  // Lua not enabled, Lua manager does nothing
class LuaManager {
  public:
	LuaManager(Memory& mem) {}

	void close() {}
	void initialize() {}
	void loadFile(const char* path) {}
	void reset() {}
};
#endif