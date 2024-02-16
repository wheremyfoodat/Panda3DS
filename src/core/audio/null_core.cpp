#include "audio/null_core.hpp"

namespace Audio {
	namespace DSPPipeType {
		enum : u32 {
			Debug = 0,
			DMA = 1,
			Audio = 2,
			Binary = 3,
		};
	}

	void NullDSP::resetAudioPipe() {
		// Hardcoded responses for now
		// These are DSP DRAM offsets for various variables
		// https://www.3dbrew.org/wiki/DSP_Memory_Region
		static constexpr std::array<u16, 16> responses = {
			0x000F,  // Number of responses
			0xBFFF,  // Frame counter
			0x9E92,  // Source configs
			0x8680,  // Source statuses
			0xA792,  // ADPCM coefficients
			0x9430,  // DSP configs
			0x8400,  // DSP status
			0x8540,  // Final samples
			0x9492,  // Intermediate mix samples
			0x8710,  // Compressor
			0x8410,  // Debug
			0xA912,  // ??
			0xAA12,  // ??
			0xAAD2,  // ??
			0xAC52,  // Surround sound biquad filter 1
			0xAC5C   // Surround sound biquad filter 2
		};

		std::vector<u8>& audioPipe = pipeData[DSPPipeType::Audio];
		audioPipe.resize(responses.size() * sizeof(u16));

		// Push back every response to the audio pipe
		size_t index = 0;
		for (auto e : responses) {
			audioPipe[index++] = e & 0xff;
			audioPipe[index++] = e >> 8;
		}
	}

	void NullDSP::reset() {
		for (auto& e : pipeData) e.clear();

		// Note: Reset audio pipe AFTER resetting all pipes, otherwise the new data will be yeeted
		resetAudioPipe();
	}
	
	u16 NullDSP::recvData(u32 regId) {
		if (regId != 0) {
			Helpers::panic("Audio: invalid register in null frontend");
		}

		return dspState == DSPState::On;
	}

	void NullDSP::writeProcessPipe(u32 channel, u32 size, u32 buffer) {
		enum class StateChange : u8 {
			Initialize = 0,
			Shutdown = 1,
			Wakeup = 2,
			Sleep = 3,
		};

		switch (channel) {
			case DSPPipeType::Audio: {
				if (size != 4) {
					printf("Invalid size written to DSP Audio Pipe\n");
					break;
				}

				// Get new state
				const u8 state = mem.read8(buffer);
				if (state > 3) {
					log("WriteProcessPipe::Audio: Unknown state change type");
				} else {
					switch (static_cast<StateChange>(state)) {
						case StateChange::Initialize:
							// TODO: Other initialization stuff here
							dspState = DSPState::On;
							resetAudioPipe();
							break;

						case StateChange::Shutdown:
							dspState = DSPState::Off;
							break;

						default: Helpers::panic("Unimplemented DSP audio pipe state change %d", state);
					}
				}
				break;
			}

			case DSPPipeType::Binary:
				Helpers::warn("Unimplemented write to binary pipe! Size: %d\n", size);
				break;

			default: log("Audio::NullDSP: Wrote to unimplemented pipe %d\n", channel); break;
		}
	}

	std::vector<u8> NullDSP::readPipe(u32 pipe, u32 peer, u32 size, u32 buffer) {
		if (size & 1) Helpers::panic("Tried to read odd amount of bytes from DSP pipe");
		if (pipe >= pipeCount || size > 0xffff) {
			return {};
		}

		if (pipe != DSPPipeType::Audio) {
			log("Reading from non-audio pipe! This might be broken, might need to check what pipe is being read from and implement writing to it\n");
		}

		std::vector<u8>& data = pipeData[pipe];
		size = std::min<u32>(size, data.size());  // Clamp size to the maximum available data size

		if (size == 0) {
			return {};
		}

		// Return "size" bytes from the audio pipe and erase them
		std::vector<u8> out(data.begin(), data.begin() + size);
		data.erase(data.begin(), data.begin() + size);
		return out;
	}
}  // namespace Audio
