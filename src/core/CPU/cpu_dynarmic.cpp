#ifdef CPU_DYNARMIC
#include "cpu_dynarmic.hpp"
#include "arm_defs.hpp"

CPU::CPU(Memory& mem, Kernel& kernel) : mem(mem), env(mem, kernel, *this) {
    cp15 = std::make_shared<CP15>();

    Dynarmic::A32::UserConfig config;
    config.arch_version = Dynarmic::A32::ArchVersion::v6K;
    config.callbacks = &env;
    config.coprocessors[15] = cp15;
    config.define_unpredictable_behaviour = true;
    config.global_monitor = &exclusiveMonitor;
    config.processor_id = 0;
    config.fastmem_pointer = mem.isFastmemEnabled() ? mem.getFastmemArenaBase() : nullptr;

    jit = std::make_unique<Dynarmic::A32::Jit>(config);
}

void CPU::reset() {
    setCPSR(CPSR::UserMode);
    setFPSCR(FPSCR::MainThreadDefault);
    env.totalTicks = 0;

    cp15->reset();
    cp15->setTLSBase(VirtualAddrs::TLSBase); // Set cp15 TLS pointer to the main thread's thread-local storage
    jit->Reset();
    jit->ClearCache();
    jit->Regs().fill(0);
    jit->ExtRegs().fill(0);
}

#endif // CPU_DYNARMIC