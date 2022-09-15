#pragma once
#include <array>
#include "helpers.hpp"
#include "memory.hpp"

namespace SVCResult {
	enum : u32 {
		Success = 0
	};
}

class Kernel {
	std::array<u32, 16>& regs;
	Memory& mem;

public:
	Kernel(std::array<u32, 16>& regs, Memory& mem) : regs(regs), mem(mem) {}
	void serviceSVC(u32 svc);
	void reset();

	void createAddressArbiter();
};