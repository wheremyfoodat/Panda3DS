#ifdef CPU_DYNARMIC
#include "cpu_dynarmic.hpp"

CPU::CPU(Memory& mem) : mem(mem) {
    // Execute at least 1 instruction.
    // (Note: More than one instruction may be executed.)
    env.ticks_left = 10;

    // Write some code to memory.
    env.MemoryWrite16(0, 0x0088); // lsls r0, r1, #2
    env.MemoryWrite16(2, 0x3045); // adds r0, #69
    env.MemoryWrite16(4, 0xdf45); // swi #69
    env.MemoryWrite16(6, 0xE7FE); // b +#0 (infinite loop)

    // Setup registers.
    auto& regs = jit.Regs();
    regs[0] = 1;
    regs[1] = 2;
    regs[15] = 0; // PC = 0
    setCPSR(0x00000030); // Thumb mode

    // Execute!
    jit.Run();

    // Here we would expect cpu.Regs()[0] == 77
    printf("R0: %u\n", jit.Regs()[0]);
}

void CPU::reset() {
    jit.Regs().fill(0);
    // ARM mode, all flags disabled, interrupts and aborts all enabled, user mode
    setCPSR(0x00000010);
}

#endif // CPU_DYNARMIC