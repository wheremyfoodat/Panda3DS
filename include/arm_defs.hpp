#pragma once

// Status register definitions
namespace CPSR {
    enum : u32 {
        // Privilege modes
        UserMode = 16,
        FIQMode = 17,
        IRQMode = 18,
        SVCMode = 19,
        AbortMode = 23,
        UndefMode = 27,
        SystemMode = 31,

        // If (CPSR & Thumb) the we're in thumb mode
        Thumb = 1 << 5
	};
}

namespace FPSCR {
    // FPSCR Flags
    enum : u32 {
        NFlag = (1U << 31U), // Negative condition flag
        ZFlag = (1 << 30),   // Zero condition flag
        CFlag = (1 << 29),   // Carry condition flag
        VFlag = (1 << 28),   // Overflow condition flag

        QC = (1 << 27),   // Cumulative saturation bit
        AHP = (1 << 26),   // Alternative half-precision control bit
        DefaultNan = (1 << 25),   // Default NaN mode control bit
        FlushToZero = (1 << 24),   // Flush abnormals to 0 control bit
        RmodeMask = (3 << 22),   // Rounding Mode bit mask
        StrideMask = (3 << 20),   // Vector stride bit mask
        LengthMask = (7 << 16),   // Vector length bit mask

        IDE = (1 << 15), // Input Denormal exception trap enable.
        IXE = (1 << 12), // Inexact exception trap enable
        UFE = (1 << 11), // Undeflow exception trap enable
        OFE = (1 << 10), // Overflow exception trap enable
        DZE = (1 << 9),  // Division by Zero exception trap enable
        IOE = (1 << 8),  // Invalid Operation exception trap enable

        IDC = (1 << 7), // Input Denormal cumulative exception bit
        IXC = (1 << 4), // Inexact cumulative exception bit
        UFC = (1 << 3), // Undeflow cumulative exception bit
        OFC = (1 << 2), // Overflow cumulative exception bit
        DZC = (1 << 1), // Division by Zero cumulative exception bit
        IOC = (1 << 0), // Invalid Operation cumulative exception bit

        RoundNearest = (0 << 22),
        RoundPlusInf = (1 << 22),
        RoundMinusInf = (2 << 22),
        RoundToZero = (3 << 22),

        // Default FPSCR value for threads
        ThreadDefault = DefaultNan | FlushToZero | RoundToZero | IXC
    };
}