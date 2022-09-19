#pragma once

#include "dynarmic/interface/A32/a32.h"
#include "dynarmic/interface/A32/config.h"
#include "dynarmic/interface/A32/coprocessor.h"
#include "helpers.hpp"
#include "memory.hpp"

class CP15 final : public Dynarmic::A32::Coprocessor {
    using Callback = Dynarmic::A32::Coprocessor::Callback;
    using CoprocReg = Dynarmic::A32::CoprocReg;
    using CallbackOrAccessOneWord = Dynarmic::A32::Coprocessor::CallbackOrAccessOneWord;
    using CallbackOrAccessTwoWords = Dynarmic::A32::Coprocessor::CallbackOrAccessTwoWords;

    u32 threadStoragePointer; // Pointer to thread-local storage
    u32 dummy; // MCR writes here for registers whose values are ignored

    std::optional<Callback> CompileInternalOperation(bool two, unsigned opc1,
        CoprocReg CRd, CoprocReg CRn,
        CoprocReg CRm, unsigned opc2) override {
        return std::nullopt;
    }

    CallbackOrAccessOneWord CompileSendOneWord(bool two, unsigned opc1, CoprocReg CRn,
        CoprocReg CRm, unsigned opc2) override {
        if (!two && opc1 == 0 && CRn == CoprocReg::C7 && CRm == CoprocReg::C10 && opc2 == 4) {
            return &dummy; // Normally inserts a "Data Synchronization Barrier"
        }

        if (!two && opc1 == 0 && CRn == CoprocReg::C7 && CRm == CoprocReg::C10 && opc2 == 5) {
            return &dummy; // Normally inserts a "Data Memory Barrier"
        }
        Helpers::panic("CP15: CompileSendOneWord\nopc1: %d CRn: %d CRm: %d opc2: %d\n", opc1, (int)CRn, (int)CRm, opc2);
    }

    CallbackOrAccessTwoWords CompileSendTwoWords(bool two, unsigned opc, CoprocReg CRm) override {
        return std::monostate{};
    }

    CallbackOrAccessOneWord CompileGetOneWord(bool two, unsigned opc1, CoprocReg CRn,
        CoprocReg CRm, unsigned opc2) override {
        // Some sort of pointer to TLS, accessed via mrc p15, 0, rd, c13, c0, 3
        if (!two && CRn == CoprocReg::C13 && opc1 == 0 && CRm == CoprocReg::C0 && opc2 == 3) {
            return &threadStoragePointer;
        }

        Helpers::panic("CP15: CompileGetOneWord\nopc1: %d CRn: %d CRm: %d opc2: %d\n", opc1, (int)CRn, (int)CRm, opc2);
    }

    CallbackOrAccessTwoWords CompileGetTwoWords(bool two, unsigned opc, CoprocReg CRm) override {
        return std::monostate{};
    }

    std::optional<Callback> CompileLoadWords(bool two, bool long_transfer, CoprocReg CRd,
        std::optional<u8> option) override {
        return std::nullopt;
    }

    std::optional<Callback> CompileStoreWords(bool two, bool long_transfer, CoprocReg CRd,
        std::optional<u8> option) override {
        return std::nullopt;
    }

public:
    void reset() {
        threadStoragePointer = VirtualAddrs::TLSBase;
    }
};