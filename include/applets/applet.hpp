#pragma once

#include "helpers.hpp"
#include "memory.hpp"
#include "result/result.hpp"

class AppletBase {
	Memory& mem;

  public:
	// Called by APT::StartLibraryApplet and similar
	virtual Result::HorizonResult start() = 0;
	virtual void reset() = 0;

	AppletBase(Memory& mem) : mem(mem) {}
};