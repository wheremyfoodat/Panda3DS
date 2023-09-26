#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

class NIMService {
	Memory& mem;
	MAKE_LOG_FUNCTION(log, nimLogger)

	// Service commands
	void initialize(u32 messagePointer);

public:
	enum class Type {
		AOC,  // nim:aoc
		U,    // nim:u
	};

	NIMService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer, Type type);
};