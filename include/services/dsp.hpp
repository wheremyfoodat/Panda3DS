#pragma once
#include <array>
#include <filesystem>
#include <optional>
#include <vector>

#include "audio/dsp_core.hpp"
#include "helpers.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

// Circular dependencies!
class Kernel;

class DSPService {
	Handle handle = KernelHandles::DSP;
	Memory& mem;
	Kernel& kernel;
	Audio::DSPCore* dsp = nullptr;
	MAKE_LOG_FUNCTION(log, dspServiceLogger)

	// Number of DSP pipes
	static constexpr size_t pipeCount = 8;

	// DSP service event handles
	using DSPEvent = std::optional<Handle>;

	DSPEvent semaphoreEvent;
	DSPEvent interrupt0;
	DSPEvent interrupt1;
	std::array<DSPEvent, pipeCount> pipeEvents;
	u16 semaphoreMask = 0;

	DSPEvent& getEventRef(u32 type, u32 pipe);
	static constexpr size_t maxEventCount = 6;

	// Total number of DSP service events registered with registerInterruptEvents
	size_t totalEventCount;
	std::vector<u8> loadedComponent;

	// Service functions
	void convertProcessAddressFromDspDram(u32 messagePointer);  // Nice function name
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
	void setDSPCore(Audio::DSPCore* pointer) { dsp = pointer; }

	// Special callback that's ran when the semaphore event is signalled
	void onSemaphoreEventSignal() { dsp->setSemaphore(semaphoreMask); }

	enum class SoundOutputMode : u8 {
		Mono = 0,
		Stereo = 1,
		Surround = 2,
	};

	enum class ComponentDumpResult : u8 {
		Success = 0,
		NotLoaded,
		FileFailure,
	};

	void triggerPipeEvent(int index);
	void triggerSemaphoreEvent();
	void triggerInterrupt0();
	void triggerInterrupt1();

	ComponentDumpResult dumpComponent(const std::filesystem::path& path);
};