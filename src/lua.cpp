#ifdef PANDA3DS_ENABLE_LUA
#include <teakra/disassembler.h>

#include <array>

#include "capstone.hpp"
#include "emulator.hpp"
#include "lua_manager.hpp"

#ifndef __ANDROID__
extern "C" {
#include "luv.h"
}
#endif

void LuaManager::initialize() {
	L = luaL_newstate();  // Open Lua

	if (!L) {
		printf("Lua initialization failed, continuing without Lua");
		initialized = false;
		return;
	}
	luaL_openlibs(L);

#ifndef __ANDROID__
	lua_pushstring(L, "luv");
	luaopen_luv(L);
	lua_settable(L, LUA_GLOBALSINDEX);
#endif

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

void LuaManager::loadString(const std::string& code) {
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

	int status = luaL_loadstring(L, code.c_str());  // load Lua script
	int ret = lua_pcall(L, 0, 0, 0);                // tell Lua to run the script

	if (ret != 0) {
		haveScript = false;
		fprintf(stderr, "%s\n", lua_tostring(L, -1));  // tell us what mistake we made
	} else {
		haveScript = true;
	}
}

void LuaManager::signalEventInternal(LuaEvent e) {
	lua_getglobal(L, "eventHandler");        // We want to call the event handler
	lua_pushnumber(L, static_cast<int>(e));  // Push event type

	// Call the function with 1 argument and 0 outputs, without an error handler
	lua_pcall(L, 1, 0, 0);
}

void LuaManager::reset() {
	// Reset scripts
	haveScript = false;
}

// Initialize C++ thunks for Lua code to call here
// All code beyond this point is terrible and full of global state, don't judge

Emulator* LuaManager::g_emulator = nullptr;

#define MAKE_MEMORY_FUNCTIONS(size)                                               \
	static int read##size##Thunk(lua_State* L) {                                  \
		const u32 vaddr = (u32)lua_tonumber(L, 1);                                \
		lua_pushnumber(L, LuaManager::g_emulator->getMemory().read##size(vaddr)); \
		return 1;                                                                 \
	}                                                                             \
	static int write##size##Thunk(lua_State* L) {                                 \
		const u32 vaddr = (u32)lua_tonumber(L, 1);                                \
		const u##size value = (u##size)lua_tonumber(L, 2);                        \
		LuaManager::g_emulator->getMemory().write##size(vaddr, value);            \
		return 0;                                                                 \
	}

MAKE_MEMORY_FUNCTIONS(8)
MAKE_MEMORY_FUNCTIONS(16)
MAKE_MEMORY_FUNCTIONS(32)
MAKE_MEMORY_FUNCTIONS(64)
#undef MAKE_MEMORY_FUNCTIONS

static int getAppIDThunk(lua_State* L) {
	std::optional<u64> id = LuaManager::g_emulator->getMemory().getProgramID();
	
	// If the app has an ID, return true + its ID
	// Otherwise return false and 0 as the ID
	if (id.has_value()) {
		lua_pushboolean(L, 1);    // Return true
		lua_pushnumber(L, u32(*id));  // Return bottom 32 bits
		lua_pushnumber(L, u32(*id >> 32));  // Return top 32 bits
	} else {
		lua_pushboolean(L, 0);  // Return false
		// Return no ID
		lua_pushnumber(L, 0);
		lua_pushnumber(L, 0);
	}

	return 3;
}

static int pauseThunk(lua_State* L) {
	LuaManager::g_emulator->pause();
	return 0;
}

static int resumeThunk(lua_State* L) {
	LuaManager::g_emulator->resume();
	return 0;
}

static int resetThunk(lua_State* L) {
	LuaManager::g_emulator->reset(Emulator::ReloadOption::Reload);
	return 0;
}

static int loadROMThunk(lua_State* L) {
	// Path argument is invalid, report that loading failed and exit
	if (lua_type(L, -1) != LUA_TSTRING) {
		lua_pushboolean(L, 0);
		return 1;
	}

	size_t pathLength;
	const char* const str = lua_tolstring(L, -1, &pathLength);

	const auto path = std::filesystem::path(std::string(str, pathLength));
	// Load ROM and reply if it succeeded or not
	lua_pushboolean(L, LuaManager::g_emulator->loadROM(path) ? 1 : 0);
	return 1;
}

static int getButtonsThunk(lua_State* L) {
	auto buttons = LuaManager::g_emulator->getServiceManager().getHID().getOldButtons();
	lua_pushinteger(L, static_cast<lua_Integer>(buttons));

	return 1;
}

static int getCirclepadThunk(lua_State* L) {
	auto& hid = LuaManager::g_emulator->getServiceManager().getHID();
	s16 x = hid.getCirclepadX();
	s16 y = hid.getCirclepadY();

	lua_pushinteger(L, static_cast<lua_Number>(x));
	lua_pushinteger(L, static_cast<lua_Number>(y));
	return 2;
}

static int getButtonThunk(lua_State* L) {
	auto& hid = LuaManager::g_emulator->getServiceManager().getHID();
	// This function accepts a mask. You can use it to check if one or more buttons are pressed at a time
	const u32 mask = (u32)lua_tonumber(L, 1);
	const bool result = (hid.getOldButtons() & mask) == mask;

	// Return whether the selected buttons are all pressed
	lua_pushboolean(L, result ? 1 : 0);
	return 1;
}

static int disassembleARMThunk(lua_State* L) {
	static Common::CapstoneDisassembler disassembler;
	// We want the disassembler to only be fully initialized when this function is first used
	if (!disassembler.isInitialized()) {
		disassembler.init(CS_ARCH_ARM, CS_MODE_ARM);
	}

	const u32 pc = u32(lua_tonumber(L, 1));
	const u32 instruction = u32(lua_tonumber(L, 2));

	std::string disassembly;
	// Convert instruction to byte array to pass to Capstone
	std::array<u8, 4> bytes = {
		u8(instruction & 0xff),
		u8((instruction >> 8) & 0xff),
		u8((instruction >> 16) & 0xff),
		u8((instruction >> 24) & 0xff),
	};

	disassembler.disassemble(disassembly, pc, std::span(bytes));
	lua_pushstring(L, disassembly.c_str());

	return 1;
}

static int disassembleTeakThunk(lua_State* L) {
	const u16 instruction = u16(lua_tonumber(L, 1));
	const u16 expansion = u16(lua_tonumber(L, 2));
	
	std::string disassembly = Teakra::Disassembler::Do(instruction, expansion);
	lua_pushstring(L, disassembly.c_str());
	return 1;
}

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
	{ "__getAppID", getAppIDThunk },
	{ "__pause", pauseThunk }, 
	{ "__resume", resumeThunk },
	{ "__reset", resetThunk },
	{ "__loadROM", loadROMThunk },
	{ "__getButtons", getButtonsThunk },
	{ "__getCirclepad", getCirclepadThunk },
	{ "__getButton", getButtonThunk },
	{ "__disassembleARM", disassembleARMThunk },
	{ "__disassembleTeak", disassembleTeakThunk },
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

		getAppID = function()
			local ffi = require("ffi")

			result, low, high = GLOBALS.__getAppID()
			id = bit.bor(ffi.cast("uint64_t", low), (bit.lshift(ffi.cast("uint64_t", high), 32)))
			return result, id
		end,

		pause = function() GLOBALS.__pause() end,
		resume = function() GLOBALS.__resume() end,
		reset = function() GLOBALS.__reset() end,
		loadROM = function(path) return GLOBALS.__loadROM(path) end,

		getButtons = function() return GLOBALS.__getButtons() end,
		getButton = function(button) return GLOBALS.__getButton(button) end,
		getCirclepad = function() return GLOBALS.__getCirclepad() end,

		disassembleARM = function(pc, instruction) return GLOBALS.__disassembleARM(pc, instruction) end,
		disassembleTeak = function(opcode, exp) return GLOBALS.__disassembleTeak(opcode, exp or 0) end,

		Frame = __Frame,
		ButtonA = __ButtonA,
		ButtonB = __ButtonB,
		ButtonX = __ButtonX,
		ButtonY = __ButtonY,
		ButtonL = __ButtonL,
		ButtonR = __ButtonR,
		ButtonUp = __ButtonUp,
		ButtonDown = __ButtonDown,
		ButtonLeft = __ButtonLeft,
		ButtonRight= __ButtonRight,
	}
)";

	auto addIntConstant = [&]<typename T>(T x, const char* name) {
		lua_pushinteger(L, (int)x);
		lua_setglobal(L, name);
	};

	luaL_register(L, "GLOBALS", functions);
	// Add values for event enum
	addIntConstant(LuaEvent::Frame, "__Frame");

	// Add enums for 3DS keys
	addIntConstant(HID::Keys::A, "__ButtonA");
	addIntConstant(HID::Keys::B, "__ButtonB");
	addIntConstant(HID::Keys::X, "__ButtonX");
	addIntConstant(HID::Keys::Y, "__ButtonY");
	addIntConstant(HID::Keys::Up, "__ButtonUp");
	addIntConstant(HID::Keys::Down, "__ButtonDown");
	addIntConstant(HID::Keys::Left, "__ButtonLeft");
	addIntConstant(HID::Keys::Right, "__ButtonRight");
	addIntConstant(HID::Keys::L, "__ButtonL");
	addIntConstant(HID::Keys::R, "__ButtonR");

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
