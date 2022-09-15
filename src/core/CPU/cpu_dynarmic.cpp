#ifdef CPU_DYNARMIC
#include "cpu_dynarmic.hpp"

CPU::CPU(Memory& mem, Kernel& kernel) : mem(mem), env(mem, kernel) {
    Dynarmic::A32::UserConfig config;

    config.callbacks = &env;
    config.coprocessors[15] = std::make_shared<CP15>();
    jit = std::make_unique<Dynarmic::A32::Jit>(config);
}

void CPU::reset() {
    // ARM mode, all flags disabled, interrupts and aborts all enabled, user mode

    setCPSR(0x00000010);

    jit->Reset();
    jit->ClearCache();
    jit->Regs().fill(0);
}

#endif // CPU_DYNARMIC