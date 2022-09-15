#pragma once

#include "dynarmic/interface/A32/a32.h"
#include "dynarmic/interface/A32/config.h"
#include "helpers.hpp"

class MyEnvironment final : public Dynarmic::A32::UserCallbacks {
public:
    u64 ticks_left = 0;
    std::array<u8, 2048> memory{};

    u8 MemoryRead8(u32 vaddr) override {
        if (vaddr >= memory.size()) {
            return 0;
        }
        return memory[vaddr];
    }

    u16 MemoryRead16(u32 vaddr) override {
        return u16(MemoryRead8(vaddr)) | u16(MemoryRead8(vaddr + 1)) << 8;
    }

    u32 MemoryRead32(u32 vaddr) override {
        return u32(MemoryRead16(vaddr)) | u32(MemoryRead16(vaddr + 2)) << 16;
    }

    u64 MemoryRead64(u32 vaddr) override {
        return u64(MemoryRead32(vaddr)) | u64(MemoryRead32(vaddr + 4)) << 32;
    }

    void MemoryWrite8(u32 vaddr, u8 value) override {
        if (vaddr >= memory.size()) {
            return;
        }
        memory[vaddr] = value;
    }

    void MemoryWrite16(u32 vaddr, u16 value) override {
        MemoryWrite8(vaddr, u8(value));
        MemoryWrite8(vaddr + 1, u8(value >> 8));
    }

    void MemoryWrite32(u32 vaddr, u32 value) override {
        MemoryWrite16(vaddr, u16(value));
        MemoryWrite16(vaddr + 2, u16(value >> 16));
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
        Helpers::panic("Called SVC %d", swi);
    }

    void ExceptionRaised(u32 pc, Dynarmic::A32::Exception exception) override {
        Helpers::panic("Fired exception oops");
    }

    void AddTicks(u64 ticks) override {
        if (ticks > ticks_left) {
            ticks_left = 0;
            return;
        }
        ticks_left -= ticks;
    }

    u64 GetTicksRemaining() override {
        return ticks_left;
    }
};

class CPU {
    MyEnvironment env;
    Dynarmic::A32::Jit jit{ {.callbacks = &env} };

public:
    CPU();

    void setReg(int index, u32 value) {
        jit.Regs()[index] = value;
    }

    u32 getReg(int index) {
        return jit.Regs()[index];
    }

    std::array<u32, 16>& regs() {
        return jit.Regs();
    }

    void setCPSR(u32 value) {
        jit.SetCpsr(value);
    }

    u32 getCPSR() {
        return jit.Cpsr();
    }
};