#ifdef PANDA3DS_ENABLE_LUA
#include "lua_manager.hpp"

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
	haveScript = false;
}

void LuaManager::close() {
	if (initialized) {
		lua_close(L);
		initialized = false;
		haveScript = false;
		L = nullptr;
	}
}

void LuaManager::loadFile(const char* path) {
	// Initialize Lua if it has not been initialized
	if (!initialized) {
		initialize();
	}
	
	// If init failed, don't execute
	if (!initialized) {
		printf("Lua initialization failed, file won't run\n");
		haveScript = false;

		return;
	}

	int status = luaL_loadfile(L, path);  // load Lua script
	int ret = lua_pcall(L, 0, 0, 0);      // tell Lua to run the script

	if (ret != 0) {
		haveScript = false;
		fprintf(stderr, "%s\n", lua_tostring(L, -1));  // tell us what mistake we made
	} else {
		haveScript = true;
	}
}

void LuaManager::signalEventInternal(LuaEvent e) {
	lua_getglobal(L, "eventHandler"); // We want to call the event handler
	lua_pushnumber(L, static_cast<int>(e)); // Push event type

	// Call the function with 1 argument and 0 outputs, without an error handler
	lua_pcall(L, 1, 0, 0);
}

void LuaManager::reset() {
	// Reset scripts
	haveScript = false;
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

// clang-format off
static constexpr luaL_Reg functions[] = {
	{ "__read8", read8Thunk },
	{ "__read16", read16Thunk },
	{ "__read32", read32Thunk },
	{ "__read64", read64Thunk },
	{ "__write8", write8Thunk} ,
	{ "__write16", write16Thunk },
	{ "__write32", write32Thunk },
	{ "__write64", write64Thunk },
	{ nullptr, nullptr },
};
// clang-format on

void LuaManager::initializeThunks() {
	static const char* runtimeInit = R"(
	Pand = {
		read8 = function(addr) return GLOBALS.__read8(addr) end,
		read16 = function(addr) return GLOBALS.__read16(addr) end,
		read32 = function(addr) return GLOBALS.__read32(addr) end,
		read64 = function(addr) return GLOBALS.__read64(addr) end,
		write8 = function(addr, value) GLOBALS.__write8(addr, value) end,
		write16 = function(addr, value) GLOBALS.__write16(addr, value) end,
		write32 = function(addr, value) GLOBALS.__write32(addr, value) end,
		write64 = function(addr, value) GLOBALS.__write64(addr, value) end,
		Frame = __Frame,
	}
)";

	auto addIntConstant = [&]<typename T>(T x, const char* name) {
		lua_pushinteger(L, (int)x);
		lua_setglobal(L, name);
	};

	luaL_register(L, "GLOBALS", functions);
	addIntConstant(LuaEvent::Frame, "__Frame");

	// Call our Lua runtime initialization before any Lua script runs
	luaL_loadstring(L, runtimeInit);
	int ret = lua_pcall(L, 0, 0, 0);  // tell Lua to run the script

	if (ret != 0) {
		initialized = false;
		fprintf(stderr, "%s\n", lua_tostring(L, -1));  // Init should never fail!
	} else {
		initialized = true;
	}
}

#endif