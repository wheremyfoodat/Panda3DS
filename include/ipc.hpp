#pragma once
#include <cstdint>

namespace IPC {
	namespace BufferType {
		enum : std::uint32_t {
			Send = 1,
			Receive = 2,
		};
	}

	constexpr std::uint32_t responseHeader(std::uint32_t commandID, std::uint32_t normalResponses, std::uint32_t translateResponses) {
		// TODO: Maybe validate the response count stuff fits in 6 bits
		return (commandID << 16) | (normalResponses << 6) | translateResponses;
	}

	constexpr std::uint32_t pointerHeader(std::uint32_t index, std::uint32_t size, std::uint32_t type) {
		return (size << 14) | (index << 10) | (type << 1);
	}
}  // namespace IPC