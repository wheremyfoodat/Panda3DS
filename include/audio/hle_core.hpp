#pragma once
#include <array>
#include <cassert>
#include <deque>
#include <memory>
#include <queue>
#include <vector>

#include "audio/aac.hpp"
#include "audio/aac_decoder.hpp"
#include "audio/audio_interpolation.hpp"
#include "audio/dsp_core.hpp"
#include "audio/dsp_shared_mem.hpp"
#include "audio/hle_mixer.hpp"
#include "memory.hpp"

namespace Audio {
	struct DSPSource {
		// Audio buffer information
		// https://www.3dbrew.org/wiki/DSP_Memory_Region
		struct Buffer {
			u32 paddr;        // Physical address of the buffer
			u32 sampleCount;  // Total number of samples
			u8 adpcmScale;    // ADPCM predictor/scale
			u8 pad1;          // Unknown

			std::array<s16, 2> previousSamples;  // ADPCM y[n-1] and y[n-2]
			bool adpcmDirty;
			bool looping;
			u16 bufferID;
			u8 pad2;

			u32 playPosition = 0;  // Current position in the buffer
			SampleFormat format;
			SourceType sourceType;

			bool fromQueue = false;      // Is this buffer from the buffer queue or an embedded buffer?
			bool hasPlayedOnce = false;  // Has the buffer been played at least once before?

			bool operator<(const Buffer& other) const {
				// Lower ID = Higher priority
				// If this buffer ID is greater than the other one, then this buffer has a lower priority
				return this->bufferID > other.bufferID;
			}
		};

		// Buffer of decoded PCM16 samples. TODO: Are there better alternatives to use over deque?
		using SampleBuffer = std::deque<std::array<s16, 2>>;
		using BufferQueue = std::priority_queue<Buffer>;
		using InterpolationMode = HLE::SourceConfiguration::Configuration::InterpolationMode;
		using InterpolationState = Audio::Interpolation::State;

		// The samples this voice output for this audio frame.
		// Aligned to 4 for SIMD purposes.
		alignas(4) DSPMixer::StereoFrame<s16> currentFrame;
		BufferQueue buffers;

		SampleFormat sampleFormat = SampleFormat::ADPCM;
		SourceType sourceType = SourceType::Stereo;
		InterpolationMode interpolationMode = InterpolationMode::Linear;
		InterpolationState interpolationState;

		// There's one gain configuration for each of the 3 intermediate mixing stages
		// And each gain configuration is composed of 4 gain values, one for each sample in a quad-channel sample
		// Aligned to 16 for SIMD purposes
		alignas(16) std::array<std::array<float, 4>, 3> gains;
		// Of the 3 intermediate mix stages, typically only the first one is actually enabled and the other ones do nothing
		// Ie their gain is vec4(0.0). We track which stages are disabled (have a gain of all 0s) using this bitfield and skip them
		// In order to save up on CPU time.
		uint enabledMixStages = 0;

		u32 samplePosition;      // Sample number into the current audio buffer
		u32 currentBufferPaddr;  // Physical address of current audio buffer

		float rateMultiplier;
		u16 syncCount;
		u16 currentBufferID;
		u16 previousBufferID;

		bool enabled;                  // Is the source enabled?
		bool isBufferIDDirty = false;  // Did we change buffers?

		// ADPCM decoding info:
		// An array of fixed point S5.11 coefficients. These provide "weights" for the history samples
		// The system describing how an ADPCM output sample is generated is
		// y[n] = x[n] + 0.5 + coeff1 * y[n-1] + coeff2 * y[n-2]
		// Where y[n] is the output sample we're generating, x[n] is the ADPCM "differential" of the current sample
		// And coeff1/coeff2 are the coefficients from this array that are used for weighing the history samples
		std::array<s16, 16> adpcmCoefficients;
		s16 history1;  // y[n-1], the previous output sample
		s16 history2;  // y[n-2], the previous previous output sample

		SampleBuffer currentSamples;
		int index = 0;  // Index of the voice in [0, 23] for debugging

		void reset();

		// Push a buffer to the buffer queue
		void pushBuffer(const Buffer& buffer) { buffers.push(buffer); }

		// Pop a buffer from the buffer queue and return it
		Buffer popBuffer() {
			assert(!buffers.empty());

			Buffer ret = buffers.top();
			buffers.pop();

			return ret;
		}

		DSPSource() { reset(); }
	};

	class HLE_DSP : public DSPCore {
		// The audio frame types are public in case we want to use them for unit tests
	  public:
		template <typename T, usize channelCount = 1>
		using Sample = DSPMixer::Sample<T, channelCount>;

		template <typename T, usize channelCount>
		using Frame = DSPMixer::Frame<T, channelCount>;

		template <typename T>
		using MonoFrame = DSPMixer::MonoFrame<T>;

		template <typename T>
		using StereoFrame = DSPMixer::StereoFrame<T>;

		template <typename T>
		using QuadFrame = DSPMixer::QuadFrame<T>;

		using Source = Audio::DSPSource;
		using SampleBuffer = Source::SampleBuffer;
		using IntermediateMix = DSPMixer::IntermediateMix;

	  private:
		enum class DSPState : u32 {
			Off,
			On,
			Slep,
		};

		// Number of DSP pipes
		static constexpr size_t pipeCount = 8;
		DSPState dspState;

		std::array<std::vector<u8>, pipeCount> pipeData;      // The data of each pipe
		std::array<Source, Audio::HLE::sourceCount> sources;  // DSP voices
		Audio::HLE::DspMemory dspRam;

		Audio::DSPMixer mixer;
		std::unique_ptr<Audio::AAC::Decoder> aacDecoder;

		void resetAudioPipe();
		bool loaded = false;  // Have we loaded a component?

		// Get the index for the current region we'll be reading. Returns the region with the highest frame counter
		// Accounting for whether one of the frame counters has wrapped around
		usize readRegionIndex() const {
			const auto counter0 = dspRam.region0.frameCounter;
			const auto counter1 = dspRam.region1.frameCounter;

			// Handle wraparound cases first
			if (counter0 == 0xffff && counter1 != 0xfffe) {
				return 1;
			} else if (counter1 == 0xffff && counter0 != 0xfffe) {
				return 0;
			} else {
				return (counter0 > counter1) ? 0 : 1;
			}
		}

		// DSP shared memory is double buffered; One region is being written to while the other one is being read from
		Audio::HLE::SharedMemory& readRegion() { return readRegionIndex() == 0 ? dspRam.region0 : dspRam.region1; }
		Audio::HLE::SharedMemory& writeRegion() { return readRegionIndex() == 0 ? dspRam.region1 : dspRam.region0; }

		// Get a pointer of type T* to the data starting from physical address paddr
		template <typename T>
		T* getPointerPhys(u32 paddr, u32 size = 0) {
			if (paddr >= PhysicalAddrs::FCRAM && paddr + size <= PhysicalAddrs::FCRAMEnd) {
				u8* fcram = mem.getFCRAM();
				u32 index = paddr - PhysicalAddrs::FCRAM;

				return (T*)&fcram[index];
			} else if (paddr >= PhysicalAddrs::DSP_RAM && paddr + size <= PhysicalAddrs::DSP_RAM_End) {
				u32 index = paddr - PhysicalAddrs::DSP_RAM;
				return (T*)&dspRam.rawMemory[index];
			} else [[unlikely]] {
				Helpers::warn("[DSP] Tried to access unknown physical address: %08X", paddr);
				return nullptr;
			}
		}

		void handleAACRequest(const AAC::Message& request);
		void updateSourceConfig(Source& source, HLE::SourceConfiguration::Configuration& config, s16_le* adpcmCoefficients);
		void updateMixerConfig(HLE::SharedMemory& sharedMem);
		void generateFrame(StereoFrame<s16>& frame);
		void generateFrame(DSPSource& source);
		void outputFrame();
		// Perform the final mix, mixing the quadraphonic samples from all voices into the output audio frame
		void performMix(Audio::HLE::SharedMemory& readRegion, Audio::HLE::SharedMemory& writeRegion);

		// Decode an entire buffer worth of audio
		void decodeBuffer(DSPSource& source);

		SampleBuffer decodePCM8(const u8* data, usize sampleCount, Source& source);
		SampleBuffer decodePCM16(const u8* data, usize sampleCount, Source& source);
		SampleBuffer decodeADPCM(const u8* data, usize sampleCount, Source& source);

	  public:
		HLE_DSP(Memory& mem, Scheduler& scheduler, DSPService& dspService, EmulatorConfig& config);
		~HLE_DSP() override {}

		void reset() override;
		void runAudioFrame(u64 eventTimestamp) override;

		u8* getDspMemory() override { return dspRam.rawMemory.data(); }
		DSPCore::Type getType() override { return DSPCore::Type::HLE; }

		u16 recvData(u32 regId) override;
		bool recvDataIsReady(u32 regId) override { return true; }  // Treat data as always ready
		void writeProcessPipe(u32 channel, u32 size, u32 buffer) override;
		std::vector<u8> readPipe(u32 channel, u32 peer, u32 size, u32 buffer) override;

		void loadComponent(std::vector<u8>& data, u32 programMask, u32 dataMask) override;
		void unloadComponent() override;
		void setSemaphore(u16 value) override {}
		void setSemaphoreMask(u16 value) override {}
	};
}  // namespace Audio
