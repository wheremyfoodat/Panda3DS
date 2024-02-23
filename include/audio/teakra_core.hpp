#pragma once
#include "audio/dsp_core.hpp"
#include "memory.hpp"
#include "swap.hpp"
#include "teakra/teakra.h"

namespace Audio {
	class TeakraDSP : public DSPCore {
		Teakra::Teakra teakra;
		u32 pipeBaseAddr;
		bool running;  // Is the DSP running?
		bool loaded;   // Have we finished loading a binary with LoadComponent?

		// Get a pointer to a data memory address
		u8* getDataPointer(u32 address) { return getDspMemory() + Memory::DSP_DATA_MEMORY_OFFSET + address; }

		enum class PipeDirection {
			DSPtoCPU = 0,
			CPUtoDSP = 1,
		};

		// A lot of Teakra integration code, especially pipe stuff is based on Citra's integration here:
		// https://github.com/citra-emu/citra/blob/master/src/audio_core/lle/lle.cpp
		struct PipeStatus {
			// All addresses and sizes here refer to byte values, NOT 16-bit values.
			u16_le address;
			u16_le byteSize;
			u16_le readPointer;
			u16_le writePointer;
			u8 slot;
			u8 flags;

			static constexpr u16 wrapBit = 0x8000;
			static constexpr u16 pointerMask = 0x7FFF;

			bool isFull() const { return (readPointer ^ writePointer) == wrapBit; }
			bool isEmpty() const { return (readPointer ^ writePointer) == 0; }

			// isWrapped: Are read and write pointers in different memory passes.
			// true:   xxxx]----[xxxx (data is wrapping around the end of memory)
			// false:  ----[xxxx]----
			bool isWrapped() const { return (readPointer ^ writePointer) >= wrapBit; }
		};
		static_assert(sizeof(PipeStatus) == 10, "Teakra: Pipe Status size is wrong");
		static constexpr u8 pipeToSlotIndex(u8 pipe, PipeDirection direction) { return (pipe * 2) + u8(direction); }

		PipeStatus getPipeStatus(u8 pipe, PipeDirection direction) {
			PipeStatus ret;
			const u8 index = pipeToSlotIndex(pipe, direction);

			std::memcpy(&ret, getDataPointer(pipeBaseAddr * 2 + index * sizeof(PipeStatus)), sizeof(PipeStatus));
			return ret;
		}

		void updatePipeStatus(const PipeStatus& status) {
			u8 slot = status.slot;
			u8* statusAddress = getDataPointer(pipeBaseAddr * 2 + slot * sizeof(PipeStatus));

			if (slot % 2 == 0) {
				std::memcpy(statusAddress + 4, &status.readPointer, sizeof(u16));
			} else {
				std::memcpy(statusAddress + 6, &status.writePointer, sizeof(u16));
			}
		}

		bool signalledData;
		bool signalledSemaphore;

		// Run 1 slice of DSP instructions
		void runSlice() {
			if (running) {
				teakra.Run(Audio::lleSlice);
			}
		}

	  public:
		TeakraDSP(Memory& mem, Scheduler& scheduler, DSPService& dspService);

		void reset() override;

		// Run 1 slice of DSP instructions and schedule the next audio frame
		void runAudioFrame() override {
			runSlice();
			scheduler.addEvent(Scheduler::EventType::RunDSP, scheduler.currentTimestamp + Audio::lleSlice * 2);
		}

		void setAudioEnabled(bool enable) override;
		u8* getDspMemory() override { return teakra.GetDspMemory().data(); }

		u16 recvData(u32 regId) override { return teakra.RecvData(regId); }
		bool recvDataIsReady(u32 regId) override { return teakra.RecvDataIsReady(regId); }
		void setSemaphore(u16 value) override { teakra.SetSemaphore(value); }
		void setSemaphoreMask(u16 value) override { teakra.MaskSemaphore(value); }

		void writeProcessPipe(u32 channel, u32 size, u32 buffer) override;
		std::vector<u8> readPipe(u32 channel, u32 peer, u32 size, u32 buffer) override;
		void loadComponent(std::vector<u8>& data, u32 programMask, u32 dataMask) override;
		void unloadComponent() override;
	};
}  // namespace Audio
