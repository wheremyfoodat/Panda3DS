// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project / 2023 Panda3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstdint>
#include <cstdlib>
#include <type_traits>

namespace Math {

// Abstraction for GLSL vectors
template <typename T, int size>
class Vector {
    // A GLSL vector can only have 2, 3 or 4 elements
    static_assert(size == 2 || size == 3 || size == 4);
    T m_storage[size];

public:
    T& r() { return m_storage[0]; }
    T& g() { return m_storage[1]; }
    T& b() {
        static_assert(size >= 3, "Out of bounds OpenGL::Vector access");
        return m_storage[2];
    }
    T& a() {
        static_assert(size >= 4, "Out of bounds OpenGL::Vector access");
        return m_storage[3];
    }

	const T& r() const { return m_storage[0]; }
	const T& g() const { return m_storage[1]; }
	const T& b() const {
		static_assert(size >= 3, "Out of bounds OpenGL::Vector access");
		return m_storage[2];
	}
	const T& a() const {
		static_assert(size >= 4, "Out of bounds OpenGL::Vector access");
		return m_storage[3];
	}

    T& x() { return r(); }
    T& y() { return g(); }
    T& z() { return b(); }
    T& w() { return a(); }

	const T& x() const { return r(); }
	const T& y() const { return g(); }
	const T& z() const { return b(); }
	const T& w() const { return a(); }

    T& operator[](size_t index) { return m_storage[index]; }
    const T& operator[](size_t index) const { return m_storage[index]; }

    T& u() { return r(); }
    T& v() { return g(); }

    T& s() { return r(); }
    T& t() { return g(); }
    T& p() { return b(); }
    T& q() { return a(); }

    Vector(std::array<T, size> list) { std::copy(list.begin(), list.end(), &m_storage[0]); }

    Vector() {}
};

using vec2 = Vector<float, 2>;
using vec3 = Vector<float, 3>;
using vec4 = Vector<float, 4>;

using dvec2 = Vector<double, 2>;
using dvec3 = Vector<double, 3>;
using dvec4 = Vector<double, 4>;

using ivec2 = Vector<int, 2>;
using ivec3 = Vector<int, 3>;
using ivec4 = Vector<int, 4>;

using uvec2 = Vector<uint32_t, 2>;
using uvec3 = Vector<uint32_t, 3>;
using uvec4 = Vector<uint32_t, 4>;

template <class T>
struct Rectangle {
    T left{};
    T top{};
    T right{};
    T bottom{};

    constexpr Rectangle() = default;

    constexpr Rectangle(T left, T top, T right, T bottom)
        : left(left), top(top), right(right), bottom(bottom) {}

    [[nodiscard]] constexpr bool operator==(const Rectangle<T>& rhs) const {
        return (left == rhs.left) && (top == rhs.top) && (right == rhs.right) &&
               (bottom == rhs.bottom);
    }

    [[nodiscard]] constexpr bool operator!=(const Rectangle<T>& rhs) const {
        return !operator==(rhs);
    }

    [[nodiscard]] constexpr Rectangle<T> operator*(const T value) const {
        return Rectangle{left * value, top * value, right * value, bottom * value};
    }
    
    [[nodiscard]] constexpr Rectangle<T> operator/(const T value) const {
        return Rectangle{left / value, top / value, right / value, bottom / value};
    }

    [[nodiscard]] T getWidth() const {
        return std::abs(static_cast<std::make_signed_t<T>>(right - left));
    }
    
    [[nodiscard]] T getHeight() const {
        return std::abs(static_cast<std::make_signed_t<T>>(bottom - top));
    }

    [[nodiscard]] T getArea() const {
        return getWidth() * getHeight();
    }
    
    [[nodiscard]] Rectangle<T> translateX(const T x) const {
        return Rectangle{left + x, top, right + x, bottom};
    }
    
    [[nodiscard]] Rectangle<T> translateY(const T y) const {
        return Rectangle{left, top + y, right, bottom + y};
    }

    [[nodiscard]] Rectangle<T> scale(const float s) const {
        return Rectangle{left, top, static_cast<T>(left + getWidth() * s),
                         static_cast<T>(top + getHeight() * s)};
    }
};

template <typename T>
Rectangle(T, T, T, T) -> Rectangle<T>;

template <typename T>
using Rect = Rectangle<T>;

} // end namespace Math
