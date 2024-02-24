//
// Copyright (c) 2024 Kris Jusiak (kris at jusiak dot net)
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef REFLECT
#define REFLECT 1'0'2 // SemVer - should match reflect namespace

#include <algorithm>
#include <array>
#include <string_view>
#include <source_location>
#include <type_traits>
#include <tuple>

#if not defined(REFLECT_ENUM_MIN)
#define REFLECT_ENUM_MIN 0
#endif

#if not defined(REFLECT_ENUM_MAX)
#define REFLECT_ENUM_MAX 64
#endif

#define REFLECT_FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)
struct  REFLECT_STRUCT { REFLECT_STRUCT* MEMBER; enum class ENUM { VALUE }; }; // has to be in the global namespace

/**
 * Minimal static reflection library ($CXX -x c++ -std=c++20 -c reflect)
 * Try it online - https://godbolt.org/z/EPhY5M3TE
 */
namespace reflect::inline v1_0_2 {
namespace detail {
template<class T> extern const T ext{};
template<class T> struct any { template<class R> constexpr operator R() const noexcept; };
template<auto...> struct auto_ { constexpr explicit(false) auto_(auto&&...) noexcept { } };
template<class T> struct ref { T& ref_; };
template<class T> ref(T&) -> ref<T>;
template<std::size_t N> [[nodiscard]] constexpr decltype(auto) nth_pack_element(auto&&... args) noexcept {
  return [&]<auto... Is>(std::index_sequence<Is...>) -> decltype(auto) {
    return [](auto_<Is>&&..., auto&& nth, auto&&...) -> decltype(auto) { return REFLECT_FWD(nth); }(REFLECT_FWD(args)...);
  }(std::make_index_sequence<N>{});
}

static_assert(1 == nth_pack_element<0>(1));
static_assert(1 == nth_pack_element<0>(1, 2));
static_assert(2 == nth_pack_element<1>(1, 2));
static_assert('a' == nth_pack_element<0>('a', 1, true));
static_assert(1 == nth_pack_element<1>('a', 1, true));
static_assert(true == nth_pack_element<2>('a', 1, true));

template<auto  V> [[nodiscard]] constexpr auto function_name() noexcept -> std::string_view { return std::source_location::current().function_name(); }
template<class T> [[nodiscard]] constexpr auto function_name() noexcept -> std::string_view { return std::source_location::current().function_name(); }

struct type_name_info {
  static constexpr auto name = function_name<REFLECT_STRUCT>();
  static constexpr auto begin = name.find("REFLECT_STRUCT");
  static constexpr auto end = name.substr(begin+std::size(std::string_view{"REFLECT_STRUCT"}));
};

struct enum_name_info {
  static constexpr auto name = function_name<REFLECT_STRUCT::ENUM::VALUE>();
  static constexpr auto begin = name[name.find("REFLECT_STRUCT::ENUM::VALUE")-1];
  static constexpr auto end = name.substr(name.find("REFLECT_STRUCT::ENUM::VALUE")+std::size(std::string_view{"REFLECT_STRUCT::ENUM::VALUE"}));
};

struct member_name_info {
  static constexpr auto name = function_name<ref{ext<REFLECT_STRUCT>.MEMBER}>();
  static constexpr auto begin = name[name.find("MEMBER")-1];
  static constexpr auto end = name.substr(name.find("MEMBER")+std::size(std::string_view{"MEMBER"}));
};
}  // namespace detail

constexpr auto debug(auto&&...) -> void; // [debug facility] shows types at compile time

template <class T, std::size_t Size>
struct fixed_string {
  constexpr explicit(false) fixed_string(const T* str) { std::copy_n(str, Size, std::data(data)); }
  [[nodiscard]] constexpr auto operator<=>(const fixed_string&) const = default;
  [[nodiscard]] constexpr explicit(false) operator std::string_view() const { return {std::data(data), Size}; }
  [[nodiscard]] constexpr auto size() const { return Size; }
  std::array<T, Size> data{};
};
template <class T, std::size_t Size> fixed_string(const T (&str)[Size]) -> fixed_string<T, Size-1>;

static_assert(0u == std::size(fixed_string{""}));
static_assert(fixed_string{""} == fixed_string{""});
static_assert(std::string_view{""} == std::string_view{fixed_string{""}});
static_assert(3u == std::size(fixed_string{"foo"}));
static_assert(std::string_view{"foo"} == std::string_view{fixed_string{"foo"}});
static_assert(fixed_string{"foo"} == fixed_string{"foo"});

template <class Fn, class T>
[[nodiscard]] constexpr decltype(auto) visit(Fn&& fn, T&& t, auto...) noexcept {
  #if __cpp_structured_bindings >= 202401L
    auto&& [... ts] = REFLECT_FWD(t);
    return REFLECT_FWD(fn)(REFLECT_FWD(ts)...);
  #else
    using R = std::remove_cvref_t<T>;
    using detail::any;
         if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61, _62, _63, _64] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43), REFLECT_FWD(_44), REFLECT_FWD(_45), REFLECT_FWD(_46), REFLECT_FWD(_47), REFLECT_FWD(_48), REFLECT_FWD(_49), REFLECT_FWD(_50), REFLECT_FWD(_51), REFLECT_FWD(_52), REFLECT_FWD(_53), REFLECT_FWD(_54), REFLECT_FWD(_55), REFLECT_FWD(_56), REFLECT_FWD(_57), REFLECT_FWD(_58), REFLECT_FWD(_59), REFLECT_FWD(_60), REFLECT_FWD(_61), REFLECT_FWD(_62), REFLECT_FWD(_63), REFLECT_FWD(_64)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61, _62, _63] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43), REFLECT_FWD(_44), REFLECT_FWD(_45), REFLECT_FWD(_46), REFLECT_FWD(_47), REFLECT_FWD(_48), REFLECT_FWD(_49), REFLECT_FWD(_50), REFLECT_FWD(_51), REFLECT_FWD(_52), REFLECT_FWD(_53), REFLECT_FWD(_54), REFLECT_FWD(_55), REFLECT_FWD(_56), REFLECT_FWD(_57), REFLECT_FWD(_58), REFLECT_FWD(_59), REFLECT_FWD(_60), REFLECT_FWD(_61), REFLECT_FWD(_62), REFLECT_FWD(_63)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61, _62] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43), REFLECT_FWD(_44), REFLECT_FWD(_45), REFLECT_FWD(_46), REFLECT_FWD(_47), REFLECT_FWD(_48), REFLECT_FWD(_49), REFLECT_FWD(_50), REFLECT_FWD(_51), REFLECT_FWD(_52), REFLECT_FWD(_53), REFLECT_FWD(_54), REFLECT_FWD(_55), REFLECT_FWD(_56), REFLECT_FWD(_57), REFLECT_FWD(_58), REFLECT_FWD(_59), REFLECT_FWD(_60), REFLECT_FWD(_61), REFLECT_FWD(_62)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43), REFLECT_FWD(_44), REFLECT_FWD(_45), REFLECT_FWD(_46), REFLECT_FWD(_47), REFLECT_FWD(_48), REFLECT_FWD(_49), REFLECT_FWD(_50), REFLECT_FWD(_51), REFLECT_FWD(_52), REFLECT_FWD(_53), REFLECT_FWD(_54), REFLECT_FWD(_55), REFLECT_FWD(_56), REFLECT_FWD(_57), REFLECT_FWD(_58), REFLECT_FWD(_59), REFLECT_FWD(_60), REFLECT_FWD(_61)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _60] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43), REFLECT_FWD(_44), REFLECT_FWD(_45), REFLECT_FWD(_46), REFLECT_FWD(_47), REFLECT_FWD(_48), REFLECT_FWD(_49), REFLECT_FWD(_50), REFLECT_FWD(_51), REFLECT_FWD(_52), REFLECT_FWD(_53), REFLECT_FWD(_54), REFLECT_FWD(_55), REFLECT_FWD(_56), REFLECT_FWD(_57), REFLECT_FWD(_58), REFLECT_FWD(_59), REFLECT_FWD(_60)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43), REFLECT_FWD(_44), REFLECT_FWD(_45), REFLECT_FWD(_46), REFLECT_FWD(_47), REFLECT_FWD(_48), REFLECT_FWD(_49), REFLECT_FWD(_50), REFLECT_FWD(_51), REFLECT_FWD(_52), REFLECT_FWD(_53), REFLECT_FWD(_54), REFLECT_FWD(_55), REFLECT_FWD(_56), REFLECT_FWD(_57), REFLECT_FWD(_58), REFLECT_FWD(_59)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43), REFLECT_FWD(_44), REFLECT_FWD(_45), REFLECT_FWD(_46), REFLECT_FWD(_47), REFLECT_FWD(_48), REFLECT_FWD(_49), REFLECT_FWD(_50), REFLECT_FWD(_51), REFLECT_FWD(_52), REFLECT_FWD(_53), REFLECT_FWD(_54), REFLECT_FWD(_55), REFLECT_FWD(_56), REFLECT_FWD(_57), REFLECT_FWD(_58)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43), REFLECT_FWD(_44), REFLECT_FWD(_45), REFLECT_FWD(_46), REFLECT_FWD(_47), REFLECT_FWD(_48), REFLECT_FWD(_49), REFLECT_FWD(_50), REFLECT_FWD(_51), REFLECT_FWD(_52), REFLECT_FWD(_53), REFLECT_FWD(_54), REFLECT_FWD(_55), REFLECT_FWD(_56), REFLECT_FWD(_57)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43), REFLECT_FWD(_44), REFLECT_FWD(_45), REFLECT_FWD(_46), REFLECT_FWD(_47), REFLECT_FWD(_48), REFLECT_FWD(_49), REFLECT_FWD(_50), REFLECT_FWD(_51), REFLECT_FWD(_52), REFLECT_FWD(_53), REFLECT_FWD(_54), REFLECT_FWD(_55), REFLECT_FWD(_56)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43), REFLECT_FWD(_44), REFLECT_FWD(_45), REFLECT_FWD(_46), REFLECT_FWD(_47), REFLECT_FWD(_48), REFLECT_FWD(_49), REFLECT_FWD(_50), REFLECT_FWD(_51), REFLECT_FWD(_52), REFLECT_FWD(_53), REFLECT_FWD(_54), REFLECT_FWD(_55)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43), REFLECT_FWD(_44), REFLECT_FWD(_45), REFLECT_FWD(_46), REFLECT_FWD(_47), REFLECT_FWD(_48), REFLECT_FWD(_49), REFLECT_FWD(_50), REFLECT_FWD(_51), REFLECT_FWD(_52), REFLECT_FWD(_53), REFLECT_FWD(_54)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43), REFLECT_FWD(_44), REFLECT_FWD(_45), REFLECT_FWD(_46), REFLECT_FWD(_47), REFLECT_FWD(_48), REFLECT_FWD(_49), REFLECT_FWD(_50), REFLECT_FWD(_51), REFLECT_FWD(_52), REFLECT_FWD(_53)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43), REFLECT_FWD(_44), REFLECT_FWD(_45), REFLECT_FWD(_46), REFLECT_FWD(_47), REFLECT_FWD(_48), REFLECT_FWD(_49), REFLECT_FWD(_50), REFLECT_FWD(_51), REFLECT_FWD(_52)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43), REFLECT_FWD(_44), REFLECT_FWD(_45), REFLECT_FWD(_46), REFLECT_FWD(_47), REFLECT_FWD(_48), REFLECT_FWD(_49), REFLECT_FWD(_50), REFLECT_FWD(_51)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43), REFLECT_FWD(_44), REFLECT_FWD(_45), REFLECT_FWD(_46), REFLECT_FWD(_47), REFLECT_FWD(_48), REFLECT_FWD(_49), REFLECT_FWD(_50)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43), REFLECT_FWD(_44), REFLECT_FWD(_45), REFLECT_FWD(_46), REFLECT_FWD(_47), REFLECT_FWD(_48), REFLECT_FWD(_49)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43), REFLECT_FWD(_44), REFLECT_FWD(_45), REFLECT_FWD(_46), REFLECT_FWD(_47), REFLECT_FWD(_48)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43), REFLECT_FWD(_44), REFLECT_FWD(_45), REFLECT_FWD(_46), REFLECT_FWD(_47)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43), REFLECT_FWD(_44), REFLECT_FWD(_45), REFLECT_FWD(_46)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43), REFLECT_FWD(_44), REFLECT_FWD(_45)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43), REFLECT_FWD(_44)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42), REFLECT_FWD(_43)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41), REFLECT_FWD(_42)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40), REFLECT_FWD(_41)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39), REFLECT_FWD(_40)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38), REFLECT_FWD(_39)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37), REFLECT_FWD(_38)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36), REFLECT_FWD(_37)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35), REFLECT_FWD(_36)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34), REFLECT_FWD(_35)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33), REFLECT_FWD(_34)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32), REFLECT_FWD(_33)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31), REFLECT_FWD(_32)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30), REFLECT_FWD(_31)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29), REFLECT_FWD(_30)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28), REFLECT_FWD(_29)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27), REFLECT_FWD(_28)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26), REFLECT_FWD(_27)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25), REFLECT_FWD(_26)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24), REFLECT_FWD(_25)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23), REFLECT_FWD(_24)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22), REFLECT_FWD(_23)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21), REFLECT_FWD(_22)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20), REFLECT_FWD(_21)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19), REFLECT_FWD(_20)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18), REFLECT_FWD(_19)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17), REFLECT_FWD(_18)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16), REFLECT_FWD(_17)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15), REFLECT_FWD(_16)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14), REFLECT_FWD(_15)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13), REFLECT_FWD(_14)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12), REFLECT_FWD(_13)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11), REFLECT_FWD(_12)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10), REFLECT_FWD(_11)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9), REFLECT_FWD(_10)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8, _9] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8), REFLECT_FWD(_9)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7, _8] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7), REFLECT_FWD(_8)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6, _7] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6), REFLECT_FWD(_7)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5, _6] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5), REFLECT_FWD(_6)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4, _5] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4), REFLECT_FWD(_5)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3, _4] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3), REFLECT_FWD(_4)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}, any<R>{}}; }) { auto&& [_1, _2, _3] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2), REFLECT_FWD(_3)); }
    else if constexpr (requires { R{any<R>{}, any<R>{}}; }) { auto&& [_1, _2] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1), REFLECT_FWD(_2)); }
    else if constexpr (requires { R{any<R>{}}; }) { auto&& [_1] = REFLECT_FWD(t); return REFLECT_FWD(fn)(REFLECT_FWD(_1)); }
    else if constexpr (requires { R{}; }) { return REFLECT_FWD(fn)(); }
  #endif
}

static_assert(([] {
  struct empty {};
  static_assert(0 == visit([]([[maybe_unused]] auto&&... args) { return sizeof...(args); }, empty{}));

  struct one { int a; };
  static_assert(1 == visit([]([[maybe_unused]] auto&&... args) { return sizeof...(args); }, one{}));

  struct two { int a; int b; };
  static_assert(2 == visit([]([[maybe_unused]] auto&&... args) { return sizeof...(args); }, two{}));
}(), true));

template<class T>
inline constexpr auto size = visit([]([[maybe_unused]] auto&&... args) { return sizeof...(args); }, detail::ext<T>);

static_assert(([] {
  struct empty {};
  static_assert(0 == size<empty>);

  struct one { int a; };
  static_assert(1 == size<one>);

  struct two { int a; int b; };
  static_assert(2 == size<two>);
}(), true));

template <class T>
[[nodiscard]] constexpr auto type_name(const T& = {}) noexcept {
  constexpr std::string_view function_name = detail::function_name<T>();
  constexpr std::string_view qualified_type_name = function_name.substr(detail::type_name_info::begin, function_name.find(detail::type_name_info::end) - detail::type_name_info::begin);
  constexpr std::string_view tmp_type_name = qualified_type_name.substr(0, qualified_type_name.find_first_of("<"));
  constexpr std::string_view type_name = tmp_type_name.substr(tmp_type_name.find_last_of("::")+1);
  static_assert(std::size(type_name) > 0u);
  return fixed_string<std::remove_cvref_t<decltype(type_name[0])>, std::size(type_name)>{std::data(type_name)};
}

namespace test {
struct empty { };
struct foo { };
struct bar { };
template <class T> struct foo_t { };
namespace ns::inline v1 {
  template<auto...> struct bar_v { };
} // ns
} // test
static_assert(([] {
  static_assert(std::string_view{"empty"} == type_name<test::empty>());
  static_assert(std::string_view{"empty"} == type_name(test::empty{}));
  static_assert(std::string_view{"foo"} == type_name<test::foo>());
  static_assert(std::string_view{"foo"} == type_name(test::foo{}));
  static_assert(std::string_view{"bar"} == type_name<test::bar>());
  static_assert(std::string_view{"bar"} == type_name(test::bar{}));
  static_assert(std::string_view{"foo_t"} == type_name<test::foo_t<void>>());
  static_assert(std::string_view{"foo_t"} == type_name<test::foo_t<int>>());
  static_assert(std::string_view{"foo_t"} == type_name<test::foo_t<test::ns::bar_v<42>>>());
  static_assert(std::string_view{"bar_v"} == type_name<test::ns::bar_v<42>>());
  static_assert(std::string_view{"bar_v"} == type_name<test::ns::bar_v<>>());
}(), true));

template <std::size_t Min = REFLECT_ENUM_MIN, std::size_t Max = REFLECT_ENUM_MAX, class E>
  requires (std::is_enum_v<E> and Max > Min)
[[nodiscard]] constexpr auto enum_name(const E e = {}) noexcept {
  std::array<std::string_view, Max-Min+1> enum_names{};
  [&]<auto... Ns>(std::index_sequence<Ns...>) {
      ([&]{
        constexpr std::string_view name = detail::function_name<static_cast<E>(Ns+Min)>();
        constexpr std::string_view tmp_name = name.substr(0, name.find(detail::enum_name_info::end));
        constexpr std::string_view enum_name = tmp_name.substr(tmp_name.find_last_of(detail::enum_name_info::begin)+1);
        enum_names[Ns] = enum_name.substr(enum_name.find_last_of("::")+1);
      }(), ...);
  }(std::make_index_sequence<std::size(enum_names)>{});
  return enum_names[static_cast<decltype(Max-Min)>(e)-Min];
}

static_assert(([] {
    enum class foobar {
      foo = 1, bar = 2
    };

    enum mask : unsigned char {
      a = 0b00,
      b = 0b01,
      c = 0b10,
    };

    enum sparse {
      _42 = 42,
      _256 = 256,
    };

    static_assert([](const auto e) { return requires { enum_name(e); }; }(foobar::foo));
    static_assert([](const auto e) { return requires { enum_name(e); }; }(mask::a));
    static_assert(not [](const auto e) { return requires { enum_name(e); }; }(0));
    static_assert(not [](const auto e) { return requires { enum_name(e); }; }(42u));

    static_assert(std::string_view{"foo"} == enum_name(foobar::foo));
    static_assert(std::string_view{"bar"} == enum_name(foobar::bar));

    static_assert(std::string_view{"a"} == enum_name(mask::a));
    static_assert(std::string_view{"b"} == enum_name(mask::b));
    static_assert(std::string_view{"c"} == enum_name(mask::c));

    static_assert(std::string_view{"_42"} == enum_name<42, 256>(sparse::_42));
    static_assert(std::string_view{"_256"} == enum_name<42, 256>(sparse::_256));

    const auto e = foobar::foo;
    static_assert(std::string_view{"foo"} == enum_name(e));
}(), true));

template <std::size_t N, class T> requires (N < size<T>)
[[nodiscard]] constexpr auto member_name(const T& = {}) noexcept {
  constexpr std::string_view function_name = detail::function_name<visit([](auto&&... args) { return detail::ref{detail::nth_pack_element<N>(REFLECT_FWD(args)...)}; }, detail::ext<T>)>();
  constexpr std::string_view tmp_member_name = function_name.substr(0, function_name.find(detail::member_name_info::end));
  constexpr std::string_view member_name = tmp_member_name.substr(tmp_member_name.find_last_of(detail::member_name_info::begin)+1);
  static_assert(std::size(member_name) > 0u);
  return fixed_string<std::remove_cvref_t<decltype(member_name[0])>, std::size(member_name)>{std::data(member_name)};
}

static_assert(([] {
  struct foo { int i; bool b; void* bar{}; };

  static_assert(std::string_view{"i"} == member_name<0, foo>());
  static_assert(std::string_view{"i"} == member_name<0>(foo{}));

  static_assert(std::string_view{"b"} == member_name<1, foo>());
  static_assert(std::string_view{"b"} == member_name<1>(foo{}));

  static_assert(std::string_view{"bar"} == member_name<2, foo>());
  static_assert(std::string_view{"bar"} == member_name<2>(foo{}));
}(), true));

template<std::size_t N, class T> requires (N < size<std::remove_cvref_t<T>>)
[[nodiscard]] constexpr decltype(auto) get(T&& t) noexcept {
  return visit([](auto&&... args) -> decltype(auto) { return detail::nth_pack_element<N>(REFLECT_FWD(args)...); }, REFLECT_FWD(t));
}

static_assert(([] {
  struct foo { int i; bool b; };

  constexpr auto f = foo{.i=42, .b=true};

  static_assert([]<auto N> { return requires { get<N>(f); }; }.template operator()<0>());
  static_assert([]<auto N> { return requires { get<N>(f); }; }.template operator()<1>());
  static_assert(not []<auto N> { return requires { get<N>(f); }; }.template operator()<2>());

  static_assert(42 == get<0>(f));
  static_assert(true == get<1>(f));
}(), true));

template <class T, fixed_string Name>
concept has_member_name = []<auto... Ns>(std::index_sequence<Ns...>) {
  return ((std::string_view{Name} == std::string_view{member_name<Ns, std::remove_cvref_t<T>>()}) or ...);
}(std::make_index_sequence<size<std::remove_cvref_t<T>>>{});

static_assert(([] {
  struct foo { int bar; };
  static_assert(has_member_name<foo, "bar">);
  static_assert(not has_member_name<foo, "baz">);
  static_assert(not has_member_name<foo, "BAR">);
  static_assert(not has_member_name<foo, "">);
}(), true));

template<fixed_string Name, class T> requires has_member_name<T, Name>
constexpr decltype(auto) get(T&& t) noexcept {
  return visit([](auto&&... args) -> decltype(auto) {
    constexpr auto index = []<auto... Ns>(std::index_sequence<Ns...>){
      return (((std::string_view{Name} == std::string_view{member_name<Ns, std::remove_cvref_t<T>>()}) ? Ns : decltype(Ns){}) + ...);
    }(std::make_index_sequence<size<std::remove_cvref_t<T>>>{});
    return detail::nth_pack_element<index>(REFLECT_FWD(args)...);
  }, REFLECT_FWD(t));
}

static_assert(([] {
  struct foo { int i; bool b; };

  constexpr auto f = foo{.i=42, .b=true};

  static_assert([]<fixed_string Name> { return requires { get<Name>(f); }; }.template operator()<"i">());
  static_assert([]<fixed_string Name> { return requires { get<Name>(f); }; }.template operator()<"b">());
  static_assert(not []<fixed_string Name> { return requires { get<Name>(f); }; }.template operator()<"unknown">());

  static_assert(42 == get<"i">(f));
  static_assert(true == get<"b">(f));
}(), true));

template<template<class...> class R, class T>
[[nodiscard]] constexpr auto to(T&& t) noexcept {
   if constexpr (std::is_lvalue_reference_v<decltype(t)>) {
    return visit([](auto&&... args) { return R<decltype(REFLECT_FWD(args))...>{REFLECT_FWD(args)...}; }, t);
  } else {
    return visit([](auto&&... args) { return R{REFLECT_FWD(args)...}; }, t);
  }
}

static_assert(([]<auto expect = [](const bool cond) { return std::array{true}[not cond]; }> {
  struct foo { int a; int b; };

  {
    constexpr auto t = to<std::tuple>(foo{.a=4, .b=2});
    static_assert(4 == std::get<0>(t));
    static_assert(2 == std::get<1>(t));
  }

  {
    auto f = foo{.a=4, .b=2};
    auto t = to<std::tuple>(f);
    std::get<0>(t) *= 10;
    f.b = 42;
    expect(40 == std::get<0>(t) and 40 == f.a);
    expect(42 == std::get<1>(t) and 42 == f.b);
  }

  {
    const auto f = foo{.a=4, .b=2};
    auto t = to<std::tuple>(f);
    expect(f.a == std::get<0>(t));
    expect(f.b == std::get<1>(t));
  }
}(), true));

template<fixed_string... Members, class TSrc, class TDst>
constexpr auto copy(const TSrc& src, TDst& dst) noexcept -> void {
  constexpr auto contains = []([[maybe_unused]] const auto name) {
    return sizeof...(Members) == 0u or ((std::string_view{name} == std::string_view{Members}) or ...);
  };
  auto dst_view = to<std::tuple>(dst);
  [&]<auto... Ns>(std::index_sequence<Ns...>) {
    ([&] {
      if constexpr (contains(member_name<Ns, TDst>()) and requires { std::get<Ns>(dst_view) = get<member_name<Ns, TDst>()>(src); }) {
        std::get<Ns>(dst_view) = get<member_name<Ns, TDst>()>(src);
      }
    }(), ...);
  }(std::make_index_sequence<size<TDst>>{});
}

static_assert(([]<auto expect = [](const bool cond) { return std::array{true}[not cond]; }> {
  struct foo {
    int a{};
    int b{};
  };

  struct bar {
    int a{};
    int b{};
  };

  const auto f = foo{.a=1, .b=2};

  {
    bar b{};
    b.b = 42;
    copy<"a">(f, b);
    expect(b.a == f.a);
    expect(42 == b.b);
  }

  {
    bar b{};
    b.b = 42;
    copy<"a", "b">(f, b);
    expect(b.a == f.a);
    expect(b.b == f.b);
  }

  {
    bar b{};
    b.a = 42;
    copy<"b">(f, b);
    expect(42 == b.a);
    expect(b.b == f.b);
  }

  {
    bar b{};
    copy<"c">(f, b); // ignores
    expect(0 == b.a);
    expect(0 == b.b);
  }

  {
    bar b{};
    copy<"a", "c", "b">(f, b); // copies a, b; ignores c
    expect(b.a == f.a);
    expect(b.b == f.b);
  }

  {
    bar b{};
    copy(f, b);
    expect(b.a == f.a);
    expect(b.b == f.b);
  }

  {
    bar b{.a=4, .b=2};
    copy(f, b); // overwrites members
    expect(b.a == f.a);
    expect(b.b == f.b);
  }

  struct baz {
    int a{};
    int c{};
  };

  {
    baz b{};
    b.c = 42;
    copy(f, b);
    expect(b.a == f.a);
    expect(42 == b.c);
  }
}(), true));

template<class R, class T>
[[nodiscard]] constexpr auto to(T&& t) noexcept {
  R r{};
  copy(REFLECT_FWD(t), r);
  return r;
}

static_assert(([]<auto expect = [](const bool cond) { return std::array{true}[not cond]; }> {
  struct foo {
    int a{};
    int b{};
  };

  struct bar {
    int a{};
    int b{};
  };

  {
    constexpr auto b = to<bar>(foo{.a=4, .b=2});
    static_assert(4 == b.a);
    static_assert(2 == b.b);
  }

  {
    auto f = foo{.a=4, .b=2};
    auto b = to<bar>(f);
    f.a = 42;
    expect(42 == f.a);
    expect(4 == b.a);
    expect(2 == b.b);
  }

  {
    const auto f = foo{.a=4, .b=2};
    const auto b = to<bar>(f);
    expect(4 == b.a);
    expect(2 == b.b);
  }

  struct baz {
    int a{};
    int c{};
  };

  {
    auto f = foo{.a=4, .b=2};
    auto b = to<bar>(f);
    b.a = 1;
    expect(1 == b.a and 4 == f.a);
    expect(2 == b.b and 2 == f.b);
  }

  {
    const auto b = to<baz>(foo{.a=4, .b=2});
    expect(4 == b.a and 0 == b.c);
  }

  struct foobar {
    int a{};
    enum : int { } b; // strong type, type conversion disabled
  };

  {
    const auto fb = to<foobar>(foo{.a=4, .b=2});
    expect(4 == fb.a and 0 == fb.b);
  }
}(), true));
} // namespace reflect

#undef REFLECT_FWD
#endif  // REFLECT