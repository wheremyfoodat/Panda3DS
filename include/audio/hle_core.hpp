#pragma once
#include <array>

#include "audio/dsp_core.hpp"
#include "audio/dsp_shared_mem.hpp"
#include "memory.hpp"

namespace Audio {
	class HLE_DSP : public DSPCore {
		enum class DSPState : u32 {
			Off,
			On,
			Slep,
		};

		// Number of DSP pipes
		static constexpr size_t pipeCount = 8;
		DSPState dspState;

		std::array<std::vector<u8>, pipeCount> pipeData;  // The data of each pipe
		Audio::HLE::DspMemory dspRam;

		void resetAudioPipe();
		bool loaded = false; // Have we loaded a component?

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
				return counter0 > counter1 ? 0 : 0;
			}
		}

		// DSP shared memory is double buffered; One region is being written to while the other one is being read from
		Audio::HLE::SharedMemory& readRegion() { return readRegionIndex() == 0 ? dspRam.region0 : dspRam.region1; }
		Audio::HLE::SharedMemory& writeRegion() { return readRegionIndex() == 0 ? dspRam.region1 : dspRam.region0; }

	  public:
		HLE_DSP(Memory& mem, Scheduler& scheduler, DSPService& dspService) : DSPCore(mem, scheduler, dspService) {}
		~HLE_DSP() override {}

		void reset() override;
		void runAudioFrame() override;

		u8* getDspMemory() override { return dspRam.rawMemory.data(); }

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