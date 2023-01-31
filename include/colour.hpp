#pragma once
#include "helpers.hpp"

// Helpers functions for converting colour channels between bit depths
namespace Colour {
    inline static u8 convert4To8Bit(u8 c) {
        return (c << 4) | c;
    }

    inline static u8 convert5To8Bit(u8 c) {
        return (c << 3) | (c >> 2);
    }
}