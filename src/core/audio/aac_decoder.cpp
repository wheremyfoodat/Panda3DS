#include "audio/aac_decoder.hpp"

#include <aacdecoder_lib.h>

#include <vector>
using namespace Audio;

void AAC::Decoder::decode(AAC::Message& response, const AAC::Message& request, AAC::Decoder::PaddrCallback paddrCallback, bool enableAudio) {
	// Copy the command and mode fields of the request to the response
	response.command = request.command;
	response.mode = request.mode;
	response.decodeResponse.size = request.decodeRequest.size;

	// Write a dummy response at first. We'll be overwriting it later if decoding goes well
	response.resultCode = AAC::ResultCode::Success;
	response.decodeResponse.channelCount = 2;
	response.decodeResponse.sampleCount = 1024;
	response.decodeResponse.sampleRate = AAC::SampleRate::Rate48000;

	if (!isInitialized()) {
		initialize();

		// AAC decoder failed to initialize, return dummy data and return without decoding
		if (!isInitialized()) {
			Helpers::warn("Failed to initialize AAC decoder");
			return;
		}
	}

	u8* input = paddrCallback(request.decodeRequest.address);
	const u8* inputEnd = paddrCallback(request.decodeRequest.address + request.decodeRequest.size);
	u8* outputLeft = paddrCallback(request.decodeRequest.destAddrLeft);
	u8* outputRight = nullptr;

	if (input == nullptr || inputEnd == nullptr || outputLeft == nullptr) {
		Helpers::warn("Invalid pointers passed to AAC decoder");
		return;
	}

	u32 bytesValid = request.decodeRequest.size;
	u32 bufferSize = request.decodeRequest.size;

	// Each frame is 2048 samples with 2 channels
	static constexpr usize frameSize = 2048 * 2;
	std::array<s16, frameSize> frame;
	std::array<std::vector<s16>, 2> audioStreams;

	bool queriedStreamInfo = false;

	while (bytesValid != 0) {
		if (aacDecoder_Fill(decoderHandle, &input, &bufferSize, &bytesValid) != AAC_DEC_OK) {
			Helpers::warn("Failed to fill AAC decoder with samples");
			return;
		}

		auto decodeResult = aacDecoder_DecodeFrame(decoderHandle, frame.data(), frameSize, 0);

		if (decodeResult == AAC_DEC_TRANSPORT_SYNC_ERROR) {
			// https://android.googlesource.com/platform/external/aac/+/2ddc922/libAACdec/include/aacdecoder_lib.h#362
			// According to the above, if we get a sync error, we're not meant to stop decoding, but rather just continue feeding data
		} else if (decodeResult == AAC_DEC_OK) {
			auto getSampleRate = [](u32 rate) {
				switch (rate) {
					case 8000: return AAC::SampleRate::Rate8000;
					case 11025: return AAC::SampleRate::Rate11025;
					case 12000: return AAC::SampleRate::Rate12000;
					case 16000: return AAC::SampleRate::Rate16000;
					case 22050: return AAC::SampleRate::Rate22050;
					case 24000: return AAC::SampleRate::Rate24000;
					case 32000: return AAC::SampleRate::Rate32000;
					case 44100: return AAC::SampleRate::Rate44100;
					case 48000:
					default: return AAC::SampleRate::Rate48000;
				}
			};

			auto info = aacDecoder_GetStreamInfo(decoderHandle);
			response.decodeResponse.sampleCount = info->frameSize;
			response.decodeResponse.channelCount = info->numChannels;
			response.decodeResponse.sampleRate = getSampleRate(info->sampleRate);

			int channels = info->numChannels;
			// Reserve space in our output stream vectors so push_back doesn't do allocations
			for (int i = 0; i < channels; i++) {
				audioStreams[i].reserve(audioStreams[i].size() + info->frameSize);
			}

			// Fetch output pointer for right output channel if we've got > 1 channel
			if (channels > 1 && outputRight == nullptr) {
				outputRight = paddrCallback(request.decodeRequest.destAddrRight);
				// If the right output channel doesn't point to a proper padddr, return
				if (outputRight == nullptr) {
					Helpers::warn("Right AAC output channel doesn't point to valid physical address");
					return;
				}
			}

			if (enableAudio) {
				for (int sample = 0; sample < info->frameSize; sample++) {
					for (int stream = 0; stream < channels; stream++) {
						audioStreams[stream].push_back(frame[(sample * channels) + stream]);
					}
				}
			} else {
				// If audio is not enabled, push 0s
				for (int stream = 0; stream < channels; stream++) {
					audioStreams[stream].resize(audioStreams[stream].size() + info->frameSize, 0);
				}
			}
		} else {
			Helpers::warn("Failed to decode AAC frame");
			return;
		}
	}

	for (int i = 0; i < 2; i++) {
		auto& stream = audioStreams[i];
		u8* pointer = (i == 0) ? outputLeft : outputRight;

		if (!stream.empty() && pointer != nullptr) {
			std::memcpy(pointer, stream.data(), stream.size() * sizeof(s16));
		}
	}
}

void AAC::Decoder::initialize() {
	decoderHandle = aacDecoder_Open(TRANSPORT_TYPE::TT_MP4_ADTS, 1);

	if (decoderHandle == nullptr) [[unlikely]] {
		return;
	}

	// Cap output channel count to 2
	if (aacDecoder_SetParam(decoderHandle, AAC_PCM_MAX_OUTPUT_CHANNELS, 2) != AAC_DEC_OK) [[unlikely]] {
		aacDecoder_Close(decoderHandle);
		decoderHandle = nullptr;
		return;
	}
}

AAC::Decoder::~Decoder() {
	if (isInitialized()) {
		aacDecoder_Close(decoderHandle);
		decoderHandle = nullptr;
	}
}