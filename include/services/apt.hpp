#pragma once
#include "helpers.hpp"
#include "kernel_types.hpp"

class APTService {
	Handle handle = KernelHandles::APT;

public:
	void reset();
	void handleSyncRequest(u32 messagePointer);
};