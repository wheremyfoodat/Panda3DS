#pragma once
#include <array>
#include <optional>
#include "helpers.hpp"
#include "logger.hpp"
#include "memory.hpp"

// Stub for DSP audio pipe
class DSPPipe {
	// Hardcoded responses for now
	// These are DSP DRAM offsets for various variables
	// https://www.3dbrew.org/wiki/DSP_Memory_Region
	static constexpr std::array<u16, 16> pipeData = {
		0x000F, // Number of responses
		0xBFFF, // Frame counter
		0x9E92, // Source configs
		0x8680, // Source statuses
		0xA792, // ADPCM coefficients
		0x9430, // DSP configs
		0x8400, // DSP status
		0x8540, // Final samples
		0x9492, // Intermediate mix samples
		0x8710, // Compressor
		0x8410, // Debug
		0xA912, // ??
		0xAA12, // ??
		0xAAD2, // ??
		0xAC52, // Surround sound biquad filter 1
		0xAC5C  // Surround sound biquad filter 2
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

namespace DSPPipeType {
	enum : u32 {
		Debug = 0, DMA = 1, Audio = 2, Binary = 3
	};
}

// Circular dependencies!
class Kernel;

class DSPService {
	Handle handle = KernelHandles::DSP;
	Memory& mem;
	Kernel& kernel;
	MAKE_LOG_FUNCTION(log, dspServiceLogger)

	enum class DSPState : u32 {
		Off, On, Slep
	};

	// Number of DSP pipes
	static constexpr size_t pipeCount = 8;
	DSPPipe audioPipe;
	DSPState dspState;

	// DSP service event handles
	using DSPEvent = std::optional<Handle>;

	DSPEvent semaphoreEvent;
	DSPEvent interrupt0;
	DSPEvent interrupt1;
	std::array<DSPEvent, pipeCount> pipeEvents;

	DSPEvent& getEventRef(u32 type, u32 pipe);
	static constexpr size_t maxEventCount = 6;

	// Total number of DSP service events registered with registerInterruptEvents
	size_t totalEventCount;

	// Service functions
	void convertProcessAddressFromDspDram(u32 messagePointer); // Nice function name
	void flushDataCache(u32 messagePointer);
	void getHeadphoneStatus(u32 messagePointer);
	void getSemaphoreEventHandle(u32 messagePointer);
	void invalidateDCache(u32 messagePointer);
	void loadComponent(u32 messagePointer);
	void readPipeIfPossible(u32 messagePointer);
	void recvData(u32 messagePointer);
	void recvDataIsReady(u32 messagePointer);
	void registerInterruptEvents(u32 messagePointer);
	void setSemaphore(u32 messagePointer);
	void setSemaphoreMask(u32 messagePointer);
	void unloadComponent(u32 messagePointer);
	void writeProcessPipe(u32 messagePointer);

public:
	DSPService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);

	enum class SoundOutputMode : u8 {
		Mono = 0,
		Stereo = 1,
		Surround = 2
	};

	void signalEvents();
};