#pragma once
#include "helpers.hpp"

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
	void close();
	void initialize();
	void loadFile(const char* path);
	void reset();
};

#elif  // Lua not enabled, Lua manager does nothing
class LuaManager {
  public:
	void close() {}
	void initialize() {}
	void loadFile(const char* path) {}
	void reset() {}
};
#endif