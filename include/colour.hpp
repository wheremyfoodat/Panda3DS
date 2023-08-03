#pragma once
#include "types.hpp"

// Helpers functions for converting colour channels between bit depths
namespace Colour {
    inline static constexpr u8 convert4To8Bit(u8 c) {
        return (c << 4) | c;
    }

    inline static constexpr u8 convert5To8Bit(u8 c) {
        return (c << 3) | (c >> 2);
    }

    inline static constexpr u8 convert6To8Bit(u8 c) {
        return (c << 2) | (c >> 4);
    }
}
