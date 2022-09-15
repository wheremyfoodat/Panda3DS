#pragma once

#include "dynarmic/interface/A32/a32.h"
#include "dynarmic/interface/A32/config.h"
#include "dynarmic_cp15.hpp"
#include "helpers.hpp"
#include "kernel.hpp"
#include "memory.hpp"

class MyEnvironment final : public Dynarmic::A32::UserCallbacks {
public:
    u64 ticksLeft = 0;
    Memory& mem;
    Kernel& kernel;

    u8 MemoryRead8(u32 vaddr) override {
        return mem.read8(vaddr);
    }

    u16 MemoryRead16(u32 vaddr) override {
        return mem.read16(vaddr);
    }

    u32 MemoryRead32(u32 vaddr) override {
        return mem.read32(vaddr);
    }

    u64 MemoryRead64(u32 vaddr) override {
        return u64(MemoryRead32(vaddr)) | u64(MemoryRead32(vaddr + 4)) << 32;
    }

    void MemoryWrite8(u32 vaddr, u8 value) override {
        mem.write8(vaddr, value);
    }

    void MemoryWrite16(u32 vaddr, u16 value) override {
        mem.write16(vaddr, value);
    }

    void MemoryWrite32(u32 vaddr, u32 value) override {
        mem.write32(vaddr, value);
    }

    void MemoryWrite64(u32 vaddr, u64 value) override {
        MemoryWrite32(vaddr, u32(value));
        MemoryWrite32(vaddr + 4, u32(value >> 32));
    }

    void InterpreterFallback(u32 pc, size_t num_instructions) override {
        // This is never called in practice.
        std::terminate();
    }

    void CallSVC(u32 swi) override {
        kernel.serviceSVC(swi);
    }

    void ExceptionRaised(u32 pc, Dynarmic::A32::Exception exception) override {
        Helpers::panic("Fired exception oops");
    }

    void AddTicks(u64 ticks) override {
        if (ticks > ticksLeft) {
            ticksLeft = 0;
            return;
        }
        ticksLeft -= ticks;
    }

    u64 GetTicksRemaining() override {
        return ticksLeft;
    }

    MyEnvironment(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
};

class CPU {
    std::unique_ptr<Dynarmic::A32::Jit> jit;
    std::shared_ptr<CP15> cp15;
    MyEnvironment env;
    Memory& mem;

public:
    CPU(Memory& mem, Kernel& kernel);
    void reset();

    void setReg(int index, u32 value) {
        jit->Regs()[index] = value;
    }

    u32 getReg(int index) {
        return jit->Regs()[index];
    }

    std::array<u32, 16>& regs() {
        return jit->Regs();
    }

    void setCPSR(u32 value) {
        jit->SetCpsr(value);
    }

    u32 getCPSR() {
        return jit->Cpsr();
    }

    void runFrame() {
        env.ticksLeft = 268111856 / 60;
        const auto exitReason = jit->Run();
        Helpers::panic("Exit reason: %d", (u32)exitReason);
    }
};