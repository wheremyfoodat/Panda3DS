#include "audio/hle_core.hpp"

#include "services/dsp.hpp"

namespace Audio {
	namespace DSPPipeType {
		enum : u32 {
			Debug = 0,
			DMA = 1,
			Audio = 2,
			Binary = 3,
		};
	}

	void HLE_DSP::resetAudioPipe() {
#define DSPOffset(var) (0x8000 + offsetof(Audio::HLE::SharedMemory, var) / 2)

		// These are DSP shared memory offsets for various variables
		// https://www.3dbrew.org/wiki/DSP_Memory_Region
		static constexpr std::array<u16, 16> responses = {
			0x000F,                             // Number of responses
			DSPOffset(frameCounter),            // Frame counter
			DSPOffset(sourceConfigurations),    // Source configs
			DSPOffset(sourceStatuses),          // Source statuses
			DSPOffset(adpcmCoefficients),       // ADPCM coefficients
			DSPOffset(dspConfiguration),        // DSP configs
			DSPOffset(dspStatus),               // DSP status
			DSPOffset(finalSamples),            // Final samples
			DSPOffset(intermediateMixSamples),  // Intermediate mix samples
			DSPOffset(compressor),              // Compressor
			DSPOffset(dspDebug),                // Debug
			DSPOffset(unknown10),               // ??
			DSPOffset(unknown11),               // ??
			DSPOffset(unknown12),               // ??
			DSPOffset(unknown13),               // Surround sound biquad filter 1
			DSPOffset(unknown14)                // Surround sound biquad filter 2
		};
#undef DSPOffset

		std::vector<u8>& audioPipe = pipeData[DSPPipeType::Audio];
		audioPipe.resize(responses.size() * sizeof(u16));

		// Push back every response to the audio pipe
		size_t index = 0;
		for (auto e : responses) {
			audioPipe[index++] = e & 0xff;
			audioPipe[index++] = e >> 8;
		}
	}

	void HLE_DSP::reset() {
		loaded = false;
		for (auto& e : pipeData) {
			e.clear();
		}

		// Note: Reset audio pipe AFTER resetting all pipes, otherwise the new data will be yeeted
		resetAudioPipe();
	}

	void HLE_DSP::loadComponent(std::vector<u8>& data, u32 programMask, u32 dataMask) {
		if (loaded) {
			Helpers::warn("Loading DSP component when already loaded");
		}

		loaded = true;
		scheduler.addEvent(Scheduler::EventType::RunDSP, scheduler.currentTimestamp + Audio::cyclesPerFrame);
	}

	void HLE_DSP::unloadComponent() {
		if (!loaded) {
			Helpers::warn("Audio: unloadComponent called without a running program");
		}

		loaded = false;
		scheduler.removeEvent(Scheduler::EventType::RunDSP);
	}

	void HLE_DSP::runAudioFrame() {
		// Signal audio pipe when an audio frame is done
		if (dspState == DSPState::On) [[likely]] {
			dspService.triggerPipeEvent(DSPPipeType::Audio);
		}

		scheduler.addEvent(Scheduler::EventType::RunDSP, scheduler.currentTimestamp + Audio::cyclesPerFrame);
	}
	
	u16 HLE_DSP::recvData(u32 regId) {
		if (regId != 0) {
			Helpers::panic("Audio: invalid register in null frontend");
		}

		return dspState == DSPState::On;
	}

	void HLE_DSP::writeProcessPipe(u32 channel, u32 size, u32 buffer) {
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
							
							dspService.triggerPipeEvent(DSPPipeType::Audio);
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

				// This pipe and interrupt are normally used for requests like AAC decode
				dspService.triggerPipeEvent(DSPPipeType::Binary);
				break;

			default: log("Audio::HLE_DSP: Wrote to unimplemented pipe %d\n", channel); break;
		}
	}

	std::vector<u8> HLE_DSP::readPipe(u32 pipe, u32 peer, u32 size, u32 buffer) {
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
