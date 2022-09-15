#ifdef CPU_DYNARMIC
#include "cpu_dynarmic.hpp"

CPU::CPU(Memory& mem) : mem(mem), env(mem) {}

void CPU::reset() {
    // ARM mode, all flags disabled, interrupts and aborts all enabled, user mode
    setCPSR(0x00000010);

    jit.Reset();
    jit.ClearCache();
    jit.Regs().fill(0);
}

#endif // CPU_DYNARMIC