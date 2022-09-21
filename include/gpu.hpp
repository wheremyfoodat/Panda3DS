#pragma once
#include "helpers.hpp"
#include "memory.hpp"

class GPU {
	Memory& mem;

public:
	GPU(Memory& mem) : mem(mem) {}
	void clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control);
	void reset();
};