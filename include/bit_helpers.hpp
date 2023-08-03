#pragma once
#include "types.hpp"
#include <climits>

#if __cpp_lib_bit_cast
#include <bit>
#endif

namespace Helpers {
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

#if __cpp_lib_bit_cast
	template <class To, class From>
	constexpr To bit_cast(const From& from) noexcept {
		return std::bit_cast<To, From>(from);
	}
#else
	template <class To, class From>
	constexpr To bit_cast(const From& from) noexcept {
		return *reinterpret_cast<const To*>(&from);
	}
#endif

} // namespace Helpers
