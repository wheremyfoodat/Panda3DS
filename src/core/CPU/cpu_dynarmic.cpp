#ifdef CPU_DYNARMIC
#include "cpu_dynarmic.hpp"

CPU::CPU(Memory& mem, Kernel& kernel) : mem(mem), env(mem, kernel, *this) {
    cp15 = std::make_shared<CP15>();

    Dynarmic::A32::UserConfig config;
    config.arch_version = Dynarmic::A32::ArchVersion::v6K;
    config.callbacks = &env;
    config.coprocessors[15] = cp15;
    // config.define_unpredictable_behaviour = true;
    config.global_monitor = &exclusiveMonitor;
    config.processor_id = 0;
    
    jit = std::make_unique<Dynarmic::A32::Jit>(config);
}

void CPU::reset() {
    // ARM mode, all flags disabled, interrupts and aborts all enabled, user mode
    setCPSR(0x00000010);

    cp15->reset();
    jit->Reset();
    jit->ClearCache();
    jit->Regs().fill(0);
}

#endif // CPU_DYNARMIC