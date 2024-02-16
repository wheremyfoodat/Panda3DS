#pragma once
#include "audio/dsp_core.hpp"
#include "teakra/teakra.h"

namespace Audio {
	class TeakraDSP : public DSPCore {
		Teakra::Teakra teakra;
		u32 pipeBaseAddr;
		bool running;

	  public:
		TeakraDSP(Memory& mem);

		void reset() override;
		void runAudioFrame() override;
		u8* getDspMemory() override { return teakra.GetDspMemory().data(); }

		u16 recvData(u32 regId) override { return teakra.RecvData(regId); }
		bool recvDataIsReady(u32 regId) override { return teakra.RecvDataIsReady(regId); }
		void setSemaphore(u16 value) override { return teakra.SetSemaphore(value); }
		void setSemaphoreMask(u16 value) override { teakra.MaskSemaphore(value); }

		void writeProcessPipe(u32 channel, u32 size, u32 buffer) override;
		std::vector<u8> readPipe(u32 channel, u32 peer, u32 size, u32 buffer) override;
		void loadComponent(std::vector<u8>& data, u32 programMask, u32 dataMask) override;
		void unloadComponent() override;
	};
}  // namespace Audio
