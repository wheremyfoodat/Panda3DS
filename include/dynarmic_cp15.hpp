#pragma once

#include "dynarmic/interface/A32/a32.h"
#include "dynarmic/interface/A32/config.h"
#include "dynarmic/interface/A32/coprocessor.h"
#include "helpers.hpp"

class CP15 final : public Dynarmic::A32::Coprocessor {
    using Callback = Dynarmic::A32::Coprocessor::Callback;
    using CoprocReg = Dynarmic::A32::CoprocReg;
    using CallbackOrAccessOneWord = Dynarmic::A32::Coprocessor::CallbackOrAccessOneWord;
    using CallbackOrAccessTwoWords = Dynarmic::A32::Coprocessor::CallbackOrAccessTwoWords;

    u32 threadStoragePointer = 0x696966969; // Pointer to thread-local storage

    std::optional<Callback> CompileInternalOperation(bool two, unsigned opc1,
        CoprocReg CRd, CoprocReg CRn,
        CoprocReg CRm, unsigned opc2) override {
        return std::nullopt;
    }

    CallbackOrAccessOneWord CompileSendOneWord(bool two, unsigned opc1, CoprocReg CRn,
        CoprocReg CRm, unsigned opc2) override {
        Helpers::panic("CP15: CompileSendOneWord\n");
    }

    CallbackOrAccessTwoWords CompileSendTwoWords(bool two, unsigned opc, CoprocReg CRm) override {
        return std::monostate{};
    }

    CallbackOrAccessOneWord CompileGetOneWord(bool two, unsigned opc1, CoprocReg CRn,
        CoprocReg CRm, unsigned opc2) override {
        Helpers::panic("CP15: CompileGetOneWord");
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

    void reset() {

    }
};