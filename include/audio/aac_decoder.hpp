#pragma once
#include <functional>

#include "audio/aac.hpp"
#include "helpers.hpp"

struct AAC_DECODER_INSTANCE;

namespace Audio::AAC {
	class Decoder {
		using DecoderHandle = AAC_DECODER_INSTANCE*;
		using PaddrCallback = std::function<u8*(u32)>;

		DecoderHandle decoderHandle = nullptr;

		bool isInitialized() { return decoderHandle != nullptr; }
		void initialize();

	  public:
		// Decode function. Takes in a reference to the AAC response & request, and a callback for paddr -> pointer conversions
		void decode(AAC::Message& response, const AAC::Message& request, PaddrCallback paddrCallback);
		~Decoder();
	};
}  // namespace Audio::AAC