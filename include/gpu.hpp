#pragma once
#include "helpers.hpp"

class GPU {

public:
	GPU() {}
	void clearBuffer(u32 startAddress, u32 endAddress, u32 value, u32 control);
	void reset();
};