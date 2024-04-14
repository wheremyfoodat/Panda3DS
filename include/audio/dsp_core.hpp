#pragma once
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "helpers.hpp"
#include "logger.hpp"
#include "scheduler.hpp"
#include "ring_buffer.hpp"

// The DSP core must have access to the DSP service to be able to trigger interrupts properly
class DSPService;
class Memory;

namespace Audio {
	// There are 160 stereo samples in 1 audio frame, so 320 samples total
	static constexpr u64 samplesInFrame = 160;
	// 1 frame = 4096 DSP cycles = 8192 ARM11 cycles
	static constexpr u64 cyclesPerFrame = samplesInFrame * 8192;
	// For LLE DSP cores, we run the DSP for N cycles at a time, every N*2 arm11 cycles since the ARM11 runs twice as fast
	static constexpr u64 lleSlice = 16384;

	class DSPCore {
		using Samples = Common::RingBuffer<s16, 1024>;

	  protected:
		Memory& mem;
		Scheduler& scheduler;
		DSPService& dspService;

		Samples sampleBuffer;
		bool audioEnabled = false;

		MAKE_LOG_FUNCTION(log, dspLogger)

	  public:
		enum class Type { Null, Teakra, HLE };
		DSPCore(Memory& mem, Scheduler& scheduler, DSPService& dspService)
			: mem(mem), scheduler(scheduler), dspService(dspService) {}
		virtual ~DSPCore() {}

		virtual void reset() = 0;
		virtual void runAudioFrame() = 0;
		virtual u8* getDspMemory() = 0;

		virtual u16 recvData(u32 regId) = 0;
		virtual bool recvDataIsReady(u32 regId) = 0;
		virtual void setSemaphore(u16 value) = 0;
		virtual void writeProcessPipe(u32 channel, u32 size, u32 buffer) = 0;
		virtual std::vector<u8> readPipe(u32 channel, u32 peer, u32 size, u32 buffer) = 0;
		virtual void loadComponent(std::vector<u8>& data, u32 programMask, u32 dataMask) = 0;
		virtual void unloadComponent() = 0;
		virtual void setSemaphoreMask(u16 value) = 0;

		static Audio::DSPCore::Type typeFromString(std::string inString);
		static const char* typeToString(Audio::DSPCore::Type type);

		Samples& getSamples() { return sampleBuffer; }
		virtual void setAudioEnabled(bool enable) { audioEnabled = enable; }
	};

	std::unique_ptr<DSPCore> makeDSPCore(DSPCore::Type type, Memory& mem, Scheduler& scheduler, DSPService& dspService);
}  // namespace Audio