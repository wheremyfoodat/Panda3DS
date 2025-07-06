#pragma once
#include <array>

#include "helpers.hpp"
#include "io_file.hpp"
#include "nfc_types.hpp"

class AmiiboDevice {
	bool loaded = false;
	bool encrypted = false;

  public:
	static constexpr size_t tagSize = 0x21C;
	std::array<u8, tagSize> raw;

	void loadFromRaw();
	void reset();
};