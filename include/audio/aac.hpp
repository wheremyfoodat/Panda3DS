#pragma once
#include <array>
#include <type_traits>

#include "helpers.hpp"
#include "swap.hpp"

namespace Audio::AAC {
	namespace ResultCode {
		enum : u32 {
			Success = 0,
		};
	}

	// Enum values and struct definitions based off Citra
	namespace Command {
		enum : u16 {
			Init = 0,          // Initialize encoder/decoder
			EncodeDecode = 1,  // Encode/Decode AAC
			Shutdown = 2,      // Shutdown encoder/decoder
			LoadState = 3,
			SaveState = 4,
		};
	}

	namespace SampleRate {
		enum : u32 {
			Rate48000 = 0,
			Rate44100 = 1,
			Rate32000 = 2,
			Rate24000 = 3,
			Rate22050 = 4,
			Rate16000 = 5,
			Rate12000 = 6,
			Rate11025 = 7,
			Rate8000 = 8,
		};
	}

	namespace Mode {
		enum : u16 {
			None = 0,
			Decode = 1,
			Encode = 2,
		};
	}

	struct DecodeResponse {
		u32_le sampleRate;
		u32_le channelCount;
		u32_le size;
		u32_le unknown1;
		u32_le unknown2;
		u32_le sampleCount;
	};

	struct Message {
		u16_le mode = Mode::None;  // Encode or decode AAC?
		u16_le command = Command::Init;
		u32_le resultCode = ResultCode::Success;

		// Info on the AAC request
		union {
			std::array<u8, 24> commandData{};
			DecodeResponse decodeResponse;
		};
	};

	static_assert(sizeof(Message) == 32);
	static_assert(std::is_trivially_copyable<Message>());
}  // namespace Audio::AAC
