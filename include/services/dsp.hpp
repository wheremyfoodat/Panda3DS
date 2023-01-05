#pragma once
#include "helpers.hpp"
#include "logger.hpp"
#include "memory.hpp"

// Stub for DSP audio pipe
class DSPPipe {
	// Hardcoded responses for now
	static constexpr std::array<u16, 16> pipeData = {
		0x000F, //Number of responses
		0xBFFF,
		0x9E8E,
		0x8680,
		0xA78E,
		0x9430,
		0x8400,
		0x8540,
		0x948E,
		0x8710,
		0x8410,
		0xA90E,
		0xAA0E,
		0xAACE,
		0xAC4E,
		0xAC58
	};
	uint index = 0;

public:
	void reset() {
		index = 0;
	}

	// Read without checking if the pipe has overflowed
	u16 readUnchecked() {
		return pipeData[index++];
	}

	bool empty() {
		return index >= pipeData.size();
	}
};

class DSPService {
	Handle handle = KernelHandles::DSP;
	Memory& mem;
	MAKE_LOG_FUNCTION(log, dspServiceLogger)

	DSPPipe audioPipe;

	// Service functions
	void convertProcessAddressFromDspDram(u32 messagePointer); // Nice function name
	void flushDataCache(u32 messagePointer);
	void getHeadphoneStatus(u32 messagePointer);
	void getSemaphoreHandle(u32 messagePointer);
	void loadComponent(u32 messagePointer);
	void readPipeIfPossible(u32 messagePointer);
	void registerInterruptEvents(u32 messagePointer);
	void setSemaphore(u32 messagePointer);
	void setSemaphoreMask(u32 messagePointer);
	void writeProcessPipe(u32 messagePointer);

public:
	DSPService(Memory& mem) : mem(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);

	enum class SoundOutputMode : u8 {
		Mono = 0,
		Stereo = 1,
		Surround = 2
	};
};