#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

class NSService {
    Memory& mem;
	MAKE_LOG_FUNCTION(log, nsLogger)

    // Service commands
    void launchTitle(u32 messagePointer);

public:
    enum class Type {
        S,  // ns:s
        P,  // ns:p
        C,  // ns:c
    };

    NSService(Memory& mem) : mem(mem) {}
    void reset();
	void handleSyncRequest(u32 messagePointer, Type type);
};