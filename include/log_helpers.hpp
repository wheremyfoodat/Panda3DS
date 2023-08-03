#pragma once
#include <cstdarg>
#include <iostream>
#include "termcolor.hpp"

namespace Helpers {
	// Unconditional panic, unlike panicDev which does not panic on user builds
	template <class... Args>
	[[noreturn]] static void panic(const char* fmt, Args&&... args) {
		std::cout << termcolor::on_red << "[FATAL] ";
		std::printf(fmt, args...);
		std::cout << termcolor::reset << "\n";

		exit(1);
	}
	
#ifdef PANDA3DS_LIMITED_PANICS
	template <class... Args>
	static void panicDev(const char* fmt, Args&&... args) {}
#else
	template <class... Args>
	[[noreturn]] static void panicDev(const char* fmt, Args&&... args) {
		panic(fmt, args...);
	}
#endif

	template <class... Args>
	static void warn(const char* fmt, Args&&... args) {
		std::cout << termcolor::on_red << "[Warning] ";
		std::printf(fmt, args...);
		std::cout << termcolor::reset << "\n";
	}

	static constexpr bool buildingInDebugMode() {
#ifdef NDEBUG
		return false;
#endif
		return true;
	}

	static constexpr bool isUserBuild() {
#ifdef PANDA3DS_USER_BUILD
		return true;
#endif
		return false;
	}

	template <typename... Args>
	static void debug_printf(const char* fmt, Args&&... args) {
		if constexpr (buildingInDebugMode()) {
			std::printf(fmt, args...);
		}
	}
};  // namespace Helpers
