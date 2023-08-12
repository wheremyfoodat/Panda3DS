#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

#include <random>

class SSLService {
	Handle handle = KernelHandles::SSL;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, sslLogger)

	std::mt19937 rng; // Use a Mersenne Twister for RNG since this service is supposed to have better rng than just rand()
	bool initialized;

	// Service commands
	void initialize(u32 messagePointer);
	void generateRandomData(u32 messagePointer);

  public:
	SSLService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};