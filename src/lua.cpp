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
	initializeThunks();

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

// Initialize C++ thunks for Lua code to call here
// All code beyond this point is terrible and full of global state, don't judge

Memory* LuaManager::g_memory = nullptr;

#define MAKE_MEMORY_FUNCTIONS(size)                                 \
	static int read##size##Thunk(lua_State* L) {                    \
		const u32 vaddr = (u32)lua_tonumber(L, 1);                  \
		lua_pushnumber(L, LuaManager::g_memory->read##size(vaddr)); \
		return 1;                                                   \
	}                                                               \
	static int write##size##Thunk(lua_State* L) {                   \
		const u32 vaddr = (u32)lua_tonumber(L, 1);                  \
		const u##size value = (u##size)lua_tonumber(L, 2);          \
		LuaManager::g_memory->write##size(vaddr, value);            \
		return 0;                                                   \
	}


MAKE_MEMORY_FUNCTIONS(8)
MAKE_MEMORY_FUNCTIONS(16)
MAKE_MEMORY_FUNCTIONS(32)
MAKE_MEMORY_FUNCTIONS(64)
#undef MAKE_MEMORY_FUNCTIONS

void LuaManager::initializeThunks() {
	lua_register(L, "read8", read8Thunk);
	lua_register(L, "read16", read16Thunk);
	lua_register(L, "read32", read32Thunk);
	lua_register(L, "read64", read64Thunk);
	lua_register(L, "write8", write8Thunk);
	lua_register(L, "write16", write16Thunk);
	lua_register(L, "write32", write32Thunk);
	lua_register(L, "write64", write64Thunk);
}

#endif