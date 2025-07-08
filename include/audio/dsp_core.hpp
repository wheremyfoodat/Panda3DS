#pragma once
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "helpers.hpp"
#include "logger.hpp"
#include "ring_buffer.hpp"
#include "scheduler.hpp"

// The DSP core must have access to the DSP service to be able to trigger interrupts properly
class DSPService;
class Memory;
struct EmulatorConfig;

namespace Audio {
	// There are 160 stereo samples in 1 audio frame, so 320 samples total
	static constexpr u64 samplesInFrame = 160;
	// 1 frame = 4096 DSP cycles = 8192 ARM11 cycles
	static constexpr u64 cyclesPerFrame = samplesInFrame * 8192;
	// For LLE DSP cores, we run the DSP for N cycles at a time, every N*2 arm11 cycles since the ARM11 runs twice as fast
	static constexpr u64 lleSlice = 16384;

	class DSPCore {
		// 0x2000 stereo (= 2 channel) samples
		using Samples = Common::RingBuffer<s16, 0x2000 * 2>;

	  protected:
		Memory& mem;
		Scheduler& scheduler;
		DSPService& dspService;
		EmulatorConfig& settings;

		Samples sampleBuffer;
		bool audioEnabled = false;

		MAKE_LOG_FUNCTION(log, dspLogger)

	  public:
		enum class Type { Null, Teakra, HLE };
		DSPCore(Memory& mem, Scheduler& scheduler, DSPService& dspService, EmulatorConfig& settings)
			: mem(mem), scheduler(scheduler), dspService(dspService), settings(settings) {}
		virtual ~DSPCore() {}

		virtual void reset() = 0;
		virtual void runAudioFrame(u64 eventTimestamp) = 0;
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

		virtual Type getType() = 0;
		virtual void* getRegisters() { return nullptr; }

		// Read a word from program memory. By default, just perform a regular DSP RAM read for the HLE cores
		// The LLE cores translate the address, accounting for the way Teak memory is mapped
		virtual u16 readProgramWord(u32 address) {
			u8* dspRam = getDspMemory();

			auto readByte = [&](u32 addr) {
				if (addr >= 256_KB) return u8(0);
				return dspRam[addr];
			};

			u16 lsb = u16(readByte(address));
			u16 msb = u16(readByte(address + 1));
			return u16(lsb | (msb << 8));
		}
	};

	std::unique_ptr<DSPCore> makeDSPCore(EmulatorConfig& config, Memory& mem, Scheduler& scheduler, DSPService& dspService);
}  // namespace Audio