#pragma once
#include <array>

#include "helpers.hpp"
#include "io_file.hpp"

class AmiiboDevice {
  public:
	static constexpr size_t tagSize = 0x21C;

	bool loaded = false;
	std::array<u8, tagSize> raw;

	void reset();
};