#pragma once
#include <functional>
#include <memory>
#include <vector>

#include "helpers.hpp"
#include "logger.hpp"
#include "memory.hpp"

namespace Audio {
	class DSPCore {
	  protected:
		Memory& mem;
		MAKE_LOG_FUNCTION(log, dspLogger)

	  public:
		enum class Type { Null, Teakra };
		DSPCore(Memory& mem) : mem(mem) {}

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
	};

	std::unique_ptr<DSPCore> makeDSPCore(DSPCore::Type type, Memory& mem);
}  // namespace Audio