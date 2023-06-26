#pragma once
#include <type_traits>
#include <utility>

namespace Helpers {
	/// Used to make the compiler evaluate beeg loops at compile time for things like generating compile-time tables
	template <typename T, T Begin, class Func, T... Is>
	static constexpr void static_for_impl(Func&& f, std::integer_sequence<T, Is...>) {
		(f(std::integral_constant<T, Begin + Is>{}), ...);
	}

	template <typename T, T Begin, T End, class Func>
	static constexpr void static_for(Func&& f) {
		static_for_impl<T, Begin>(std::forward<Func>(f), std::make_integer_sequence<T, End - Begin>{});
	}
}