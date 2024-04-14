#include "audio/hle_core.hpp"

#include <thread>
#include <utility>

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

	HLE_DSP::HLE_DSP(Memory& mem, Scheduler& scheduler, DSPService& dspService) : DSPCore(mem, scheduler, dspService) {
		// Set up source indices
		for (int i = 0; i < sources.size(); i++) {
			sources[i].index = i;
		}
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
		dspState = DSPState::Off;
		loaded = false;

		// Initialize these to some sane defaults
		sampleFormat = SampleFormat::ADPCM;
		sourceType = SourceType::Stereo;

		for (auto& e : pipeData) {
			e.clear();
		}

		for (auto& source : sources) {
			source.reset();
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

		outputFrame();
		scheduler.addEvent(Scheduler::EventType::RunDSP, scheduler.currentTimestamp + Audio::cyclesPerFrame);
	}
	
	u16 HLE_DSP::recvData(u32 regId) {
		if (regId != 0) {
			Helpers::panic("Audio: invalid register in HLE frontend");
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

	void HLE_DSP::outputFrame() {
		StereoFrame<s16> frame;
		generateFrame(frame);

		if (audioEnabled) {
			// Wait until we've actually got room to push our frame
			while (sampleBuffer.size() + 2 > sampleBuffer.Capacity()) {
				std::this_thread::sleep_for(std::chrono::milliseconds{1});
			}

			sampleBuffer.push(frame.data(), frame.size());
		}
	}

	void HLE_DSP::generateFrame(StereoFrame<s16>& frame) {
		using namespace Audio::HLE;
		SharedMemory& read = readRegion();
		SharedMemory& write = writeRegion();

		for (int i = 0; i < sourceCount; i++) {
			// Update source configuration from the read region of shared memory
			auto& config = read.sourceConfigurations.config[i];
			auto& source = sources[i];
			updateSourceConfig(source, config);

			// Generate audio
			if (source.enabled && !source.buffers.empty()) {
				const auto& buffer = source.buffers.top();
				const u8* data = getPointerPhys<u8>(buffer.paddr);

				if (data != nullptr) {
					// TODO
				}
			}

			// Update write region of shared memory
			auto& status = write.sourceStatuses.status[i];
			status.isEnabled = source.enabled;
			status.syncCount = source.syncCount;
		}
	}

	void HLE_DSP::updateSourceConfig(Source& source, HLE::SourceConfiguration::Configuration& config) {
		// Check if the any dirty bit is set, otherwise exit early
		if (!config.dirtyRaw) {
			return;
		}

		if (config.enableDirty) {
			config.enableDirty = 0;
			source.enabled = config.enable != 0;
		}

		if (config.syncCountDirty) {
			config.syncCountDirty = 0;
			source.syncCount = config.syncCount;
		}

		if (config.resetFlag) {
			config.resetFlag = 0;
			source.reset();
		}

		if (config.partialResetFlag) {
			config.partialResetFlag = 0;
			source.buffers = {};
		}
		
		// TODO: Should we check bufferQueueDirty here too?
		if (config.formatDirty || config.embeddedBufferDirty) {
			sampleFormat = config.format;
		}

		if (config.monoOrStereoDirty || config.embeddedBufferDirty) {
			sourceType = config.monoOrStereo;
		}

		if (config.embeddedBufferDirty) {
			config.embeddedBufferDirty = 0;
			if (s32(config.length) >= 0) [[likely]] {
				// TODO: Add sample format and channel count
				Source::Buffer buffer{
					.paddr = config.physicalAddress,
					.sampleCount = config.length,
					.adpcmScale = u8(config.adpcm_ps),
					.previousSamples = {s16(config.adpcm_yn[0]), s16(config.adpcm_yn[1])},
					.adpcmDirty = config.adpcmDirty != 0,
					.looping = config.isLooping != 0,
					.bufferID = config.bufferID,
					.playPosition = config.playPosition,
					.format = sampleFormat,
					.sourceType = sourceType,
					.fromQueue = false,
					.hasPlayedOnce = false,
				};

				source.buffers.emplace(std::move(buffer));
			} else {
				log("Invalid embedded buffer size for DSP voice %d\n", source.index);
			}
		}

		if (config.partialEmbeddedBufferDirty) {
			config.partialEmbeddedBufferDirty = 0;
			printf("Partial embedded buffer dirty for voice %d\n", source.index);
		}

		if (config.bufferQueueDirty) {
			config.bufferQueueDirty = 0;
			printf("Buffer queue dirty for voice %d\n", source.index);
		}

		config.dirtyRaw = 0;
	}

	void DSPSource::reset() {
		enabled = false;
		syncCount = 0;

		buffers = {};
	}
}  // namespace Audio
