// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project / 2023 Panda3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstdlib>
#include <type_traits>

namespace Math {

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