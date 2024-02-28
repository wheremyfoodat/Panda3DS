#pragma once
#include <string>

#include "helpers.hpp"

// The kinds of events that can cause a Lua call.
// Frame: Call program on frame end
// TODO: Add more
enum class LuaEvent {
	Frame,
};

class Emulator;

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
	bool haveScript = false;

	void signalEventInternal(LuaEvent e);

  public:
	// For Lua we must have some global pointers to our emulator objects to use them in script code via thunks. See the thunks in lua.cpp as an
	// example
	static Emulator* g_emulator;

	LuaManager(Emulator& emulator) { g_emulator = &emulator; }

	void close();
	void initialize();
	void initializeThunks();
	void loadFile(const char* path);
	void loadString(const std::string& code);

	void reset();
	void signalEvent(LuaEvent e) {
		if (haveScript) [[unlikely]] {
			signalEventInternal(e);
		}
	}
};

#else  // Lua not enabled, Lua manager does nothing
class LuaManager {
  public:
	LuaManager(Emulator& emulator) {}

	void close() {}
	void initialize() {}
	void loadFile(const char* path) {}
	void loadString(const std::string& code) {}
	void reset() {}
	void signalEvent(LuaEvent e) {}
};
#endif
