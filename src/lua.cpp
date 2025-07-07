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
	lua_getglobal(L, "eventHandler");         // We want to call the event handler
	lua_pushinteger(L, static_cast<int>(e));  // Push event type

	// Call the function with 1 argument and 0 outputs, without an error handler
	lua_pcall(L, 1, 0, 0);
}

// Calls the callback passed to the addServiceIntercept function when a service call is intercepted
// It passes the service name, the function header, and a pointer to the call's TLS buffer as parameters
// The callback is expected to return a bool, indicating whether the C++ code should proceed to handle the service call
// or if the Lua code handles it entirely.
// If the bool is true, the Lua code handles the service call entirely and the C++ side doesn't do anything extra
// Otherwise, the C++ side calls its service call handling code as usual.
bool LuaManager::signalInterceptedService(const std::string& service, u32 function, u32 messagePointer, int callbackRef) {
	lua_rawgeti(L, LUA_REGISTRYINDEX, callbackRef);
	lua_pushstring(L, service.c_str());  // Push service name
	lua_pushinteger(L, function);        // Push function header
	lua_pushinteger(L, messagePointer);  // Push pointer to TLS buffer

	// Call the function with 3 arguments and 1 output, without an error handler
	const int status = lua_pcall(L, 3, 1, 0);

	if (status != LUA_OK) {
		const char* err = lua_tostring(L, -1);
		fprintf(stderr, "Lua: Error in interceptService: %s\n", err);
		lua_pop(L, 1);  // Pop error message from stack
		return false;   // Have the C++ handle the service call
	}

	// Return the value interceptService returned
	const bool ret = lua_toboolean(L, -1);
	lua_pop(L, 1);
	return ret;
}

// Removes a reference from the callback value in the registry
// Prevents memory leaks, otherwise the function object would stay forever
void LuaManager::removeInterceptedService(const std::string& service, u32 function, int callbackRef) {
	luaL_unref(L, LUA_REGISTRYINDEX, callbackRef);
}

void LuaManager::reset() {
	// Reset scripts
	haveScript = false;
}

// Initialize C++ thunks for Lua code to call here
// All code beyond this point is terrible and full of global state, don't judge

Emulator* LuaManager::g_emulator = nullptr;

#define MAKE_MEMORY_FUNCTIONS(size)                                                \
	static int read##size##Thunk(lua_State* L) {                                   \
		const u32 vaddr = (u32)lua_tointeger(L, 1);                                \
		lua_pushinteger(L, LuaManager::g_emulator->getMemory().read##size(vaddr)); \
		return 1;                                                                  \
	}                                                                              \
	static int write##size##Thunk(lua_State* L) {                                  \
		const u32 vaddr = (u32)lua_tointeger(L, 1);                                \
		const u##size value = (u##size)lua_tointeger(L, 2);                        \
		LuaManager::g_emulator->getMemory().write##size(vaddr, value);             \
		return 0;                                                                  \
	}

MAKE_MEMORY_FUNCTIONS(8)
MAKE_MEMORY_FUNCTIONS(16)
MAKE_MEMORY_FUNCTIONS(32)
MAKE_MEMORY_FUNCTIONS(64)
#undef MAKE_MEMORY_FUNCTIONS

static int readFloatThunk(lua_State* L) {
	const u32 vaddr = (u32)lua_tointeger(L, 1);
	lua_pushnumber(L, (lua_Number)Helpers::bit_cast<float, u32>(LuaManager::g_emulator->getMemory().read32(vaddr)));
	return 1;
}

static int writeFloatThunk(lua_State* L) {
	const u32 vaddr = (u32)lua_tointeger(L, 1);
	const float value = (float)lua_tonumber(L, 2);
	LuaManager::g_emulator->getMemory().write32(vaddr, Helpers::bit_cast<u32, float>(value));
	return 0;
}

static int readDoubleThunk(lua_State* L) {
	const u32 vaddr = (u32)lua_tointeger(L, 1);
	lua_pushnumber(L, (lua_Number)Helpers::bit_cast<double, u64>(LuaManager::g_emulator->getMemory().read64(vaddr)));
	return 1;
}

static int writeDoubleThunk(lua_State* L) {
	const u32 vaddr = (u32)lua_tointeger(L, 1);
	const double value = (double)lua_tonumber(L, 2);
	LuaManager::g_emulator->getMemory().write64(vaddr, Helpers::bit_cast<u64, double>(value));
	return 0;
}

static int getAppIDThunk(lua_State* L) {
	std::optional<u64> id = LuaManager::g_emulator->getMemory().getProgramID();

	// If the app has an ID, return true + its ID
	// Otherwise return false and 0 as the ID
	if (id.has_value()) {
		lua_pushboolean(L, 1);              // Return true
		lua_pushnumber(L, u32(*id));        // Return bottom 32 bits
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
	if (lua_type(L, 1) != LUA_TSTRING) {
		lua_pushboolean(L, 0);
		lua_error(L);
		return 1;
	}

	usize pathLength;
	const char* const str = lua_tolstring(L, 1, &pathLength);

	const auto path = std::filesystem::path(std::string(str, pathLength));
	// Load ROM and reply if it succeeded or not
	lua_pushboolean(L, LuaManager::g_emulator->loadROM(path) ? 1 : 0);
	return 1;
}

static int addServiceInterceptThunk(lua_State* L) {
	// Service name argument is invalid, report that loading failed and exit
	if (lua_type(L, 1) != LUA_TSTRING) {
		return luaL_error(L, "Argument 1 (service name) is not a string");
	}

	if (lua_type(L, 2) != LUA_TNUMBER) {
		return luaL_error(L, "Argument 2 (function id) is not a number");
	}

	// Callback is not a function object directly, fail and exit
	// Objects with a __call metamethod are not allowed (tables, userdata)
	// Good: addServiceIntercept(serviceName, func, myLuaFunction)
	// Good: addServiceIntercept(serviceName, func, function (service, func, buffer) ... end)
	// Bad:  addServiceIntercept(serviceName, func, obj:method)
	if (lua_type(L, 3) != LUA_TFUNCTION) {
		return luaL_error(L, "Argument 3 (callback) is not a function");
	}

	// Get the name of the service we want to intercept, as well as the header of the function to intercept
	usize nameLength;
	const char* const str = lua_tolstring(L, 1, &nameLength);
	const u32 function = (u32)lua_tointeger(L, 2);
	const auto serviceName = std::string(str, nameLength);

	// Stores a reference to the callback function object in the registry for later use
	// Must be freed with lua_unref later, in order to avoid memory leaks
	lua_pushvalue(L, 3);
	const int callbackRef = luaL_ref(L, LUA_REGISTRYINDEX);
	LuaManager::g_emulator->getServiceManager().addServiceIntercept(serviceName, function, callbackRef);
	return 0;
}

static int clearServiceInterceptsThunk(lua_State* L) {
	LuaManager::g_emulator->getServiceManager().clearServiceIntercepts();
	return 0;
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
	{ "__readFloat", readFloatThunk },
	{ "__readDouble", readDoubleThunk },
	{ "__write8", write8Thunk} ,
	{ "__write16", write16Thunk },
	{ "__write32", write32Thunk },
	{ "__write64", write64Thunk },
	{ "__writeFloat", writeFloatThunk },
	{ "__writeDouble", writeDoubleThunk },
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
	{"__addServiceIntercept", addServiceInterceptThunk },
	{"__clearServiceIntercepts", clearServiceInterceptsThunk },
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
		readFloat = function(addr) return GLOBALS.__readFloat(addr) end,
		readDouble = function(addr) return GLOBALS.__readDouble(addr) end,

		write8 = function(addr, value) GLOBALS.__write8(addr, value) end,
		write16 = function(addr, value) GLOBALS.__write16(addr, value) end,
		write32 = function(addr, value) GLOBALS.__write32(addr, value) end,
		write64 = function(addr, value) GLOBALS.__write64(addr, value) end,
		writeFloat = function(addr, value) GLOBALS.__writeFloat(addr, value) end,
		writeDouble = function(addr, value) GLOBALS.__writeDouble(addr, value) end,

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
		addServiceIntercept = function(service, func, cb) return GLOBALS.__addServiceIntercept(service, func, cb) end,
		clearServiceIntercepts = function() return GLOBALS.__clearServiceIntercepts() end,

		Frame = __Frame,
		ButtonA = __ButtonA,
		ButtonB = __ButtonB,
		ButtonX = __ButtonX,
		ButtonY = __ButtonY,
		ButtonL = __ButtonL,
		ButtonR = __ButtonR,
		ButtonZL = __ButtonZL,
		ButtonZR = __ButtonZR,
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
	addIntConstant(HID::Keys::ZL, "__ButtonZL");
	addIntConstant(HID::Keys::ZR, "__ButtonZR");

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
