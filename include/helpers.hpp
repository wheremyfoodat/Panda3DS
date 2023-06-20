#pragma once
#include <climits>
#include <cstdarg>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "termcolor.hpp"

// We have to detect and special-case AppleClang at the moment since its C++20 support is finicky and doesn't quite support std::bit_cast
#if defined(__clang__) && defined(__apple_build_version__)
#define HELPERS_APPLE_CLANG
#else
#include <bit>
#endif

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using usize = std::size_t;
using uint = unsigned int;

using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

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

	static void debug_printf(const char* fmt, ...) {
		if constexpr (buildingInDebugMode()) {
			std::va_list args;
			va_start(args, fmt);
			std::vprintf(fmt, args);
			va_end(args);
		}
	}

	/// Sign extend an arbitrary-size value to 32 bits
	static constexpr u32 inline signExtend32(u32 value, u32 startingSize) {
		auto temp = (s32)value;
		auto bitsToShift = 32 - startingSize;
		return (u32)(temp << bitsToShift >> bitsToShift);
	}

	/// Sign extend an arbitrary-size value to 16 bits
	static constexpr u16 signExtend16(u16 value, u32 startingSize) {
		auto temp = (s16)value;
		auto bitsToShift = 16 - startingSize;
		return (u16)(temp << bitsToShift >> bitsToShift);
	}

	/// Create a mask with `count` number of one bits.
	template <typename T, usize count>
	static constexpr T ones() {
		constexpr usize bitsize = CHAR_BIT * sizeof(T);
		static_assert(count <= bitsize, "count larger than bitsize of T");

		if (count == T(0)) {
			return T(0);
		}
		return static_cast<T>(~static_cast<T>(0)) >> (bitsize - count);
	}

	/// Extract bits from an integer-type
	template <usize offset, typename T>
	static constexpr T getBit(T value) {
		return (value >> offset) & T(1);
	}

	/// Extract bits from an integer-type
	template <usize offset, usize bits, typename ReturnT, typename ValueT>
	static constexpr ReturnT getBits(ValueT value) {
		static_assert((offset + bits) <= (CHAR_BIT * sizeof(ValueT)), "Invalid bit range");
		static_assert(bits > 0, "Invalid bit size");
		return ReturnT(ValueT(value >> offset) & ones<ValueT, bits>());
	}

	template <usize offset, usize bits, typename ValueT>
	static constexpr ValueT getBits(ValueT value) {
		return getBits<offset, bits, ValueT, ValueT>(value);
	}

#ifdef HELPERS_APPLE_CLANG
	template <class To, class From>
	constexpr To bit_cast(const From& from) noexcept {
		return *reinterpret_cast<const To*>(&from);
	}
#else
	template <class To, class From>
	constexpr To bit_cast(const From& from) noexcept {
		return std::bit_cast<To, From>(from);
	}
#endif

	static std::vector<std::string> split(const std::string& s, const char c) {
		std::istringstream tmp(s);
		std::vector<std::string> result(1);

		while (std::getline(tmp, *result.rbegin(), c)) {
			result.emplace_back();
		}

		// Remove temporary slot
		result.pop_back();
		return result;
	}
};  // namespace Helpers

// UDLs for memory size values
constexpr size_t operator""_KB(unsigned long long int x) { return 1024ULL * x; }
constexpr size_t operator""_MB(unsigned long long int x) { return 1024_KB * x; }
constexpr size_t operator""_GB(unsigned long long int x) { return 1024_MB * x; }

