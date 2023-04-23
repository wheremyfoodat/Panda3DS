#pragma once
#include <cstdint>

namespace IPC {
	constexpr std::uint32_t responseHeader(std::uint32_t commandID, std::uint32_t normalResponses, std::uint32_t translateResponses) {
		// TODO: Maybe validate the response count stuff fits in 6 bits
		return (commandID << 16) | (normalResponses << 6) | translateResponses;
	}
}