#pragma once
#include <array>
#include "helpers.hpp"
#include "memory.hpp"
#include "services/apt.hpp"
#include "services/hid.hpp"
#include "services/fs.hpp"
#include "services/gsp_gpu.hpp"
#include "services/gsp_lcd.hpp"

class ServiceManager {
	std::array<u32, 16>& regs;
	Memory& mem;

	APTService apt;
	HIDService hid;
	FSService fs;
	GPUService gsp_gpu;
	LCDService gsp_lcd;

	// "srv:" commands
	void getServiceHandle(u32 messagePointer);
	void registerClient(u32 messagePointer);

public:
	ServiceManager(std::array<u32, 16>& regs, Memory& mem, GPU& gpu, u32& currentPID);
	void reset();
	void handleSyncRequest(u32 messagePointer);

	// Forward a SendSyncRequest IPC message to the service with the respective handle
	void sendCommandToService(u32 messagePointer, Handle handle);
	void requestGPUInterrupt(GPUInterrupt type) { gsp_gpu.requestInterrupt(type); }
	void setGSPSharedMem(u8* ptr) { gsp_gpu.setSharedMem(ptr); }
};