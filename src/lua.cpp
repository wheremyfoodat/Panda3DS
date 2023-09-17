#ifdef PANDA3DS_ENABLE_LUA
#include "lua.hpp"

void LuaManager::initialize() {
	L = luaL_newstate();  // Open Lua
	
	if (!L) {
		printf("Lua initialization failed, continuing without Lua");
		initialized = false; 
		return;
	}

	luaL_openlibs(L);
	initialized = true;
}

void LuaManager::close() {
	if (initialized) {
		lua_close(L);
		initialized = false;
		L = nullptr;
	}
}

void LuaManager::loadFile(const char* path) {
	int status = luaL_loadfile(L, path);  // load Lua script
	int ret = lua_pcall(L, 0, 0, 0);      // tell Lua to run the script

	if (ret != 0) {
		fprintf(stderr, "%s\n", lua_tostring(L, -1));  // tell us what mistake we made
	}
}

void LuaManager::reset() {
	// Reset scripts
}
#endif