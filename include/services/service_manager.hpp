#pragma once
#include <array>
#include <optional>
#include <span>

#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "services/ac.hpp"
#include "services/act.hpp"
#include "services/am.hpp"
#include "services/apt.hpp"
#include "services/boss.hpp"
#include "services/cam.hpp"
#include "services/cecd.hpp"
#include "services/cfg.hpp"
#include "services/dlp_srvr.hpp"
#include "services/dsp.hpp"
#include "services/frd.hpp"
#include "services/fs.hpp"
#include "services/gsp_gpu.hpp"
#include "services/gsp_lcd.hpp"
#include "services/hid.hpp"
#include "services/ir_user.hpp"
#include "services/ldr_ro.hpp"
#include "services/mic.hpp"
#include "services/ndm.hpp"
#include "services/nfc.hpp"
#include "services/nim.hpp"
#include "services/ptm.hpp"
#include "services/y2r.hpp"

// More circular dependencies!!
class Kernel;

class ServiceManager {
	std::span<u32, 16> regs;
	Memory& mem;
	Kernel& kernel;

	std::optional<Handle> notificationSemaphore;

	MAKE_LOG_FUNCTION(log, srvLogger)

    ACService ac;
	ACTService act;
    AMService am;
	APTService apt;
    BOSSService boss;
	CAMService cam;
	CECDService cecd;
	CFGService cfg;
	DlpSrvrService dlp_srvr;
	DSPService dsp;
	HIDService hid;
	IRUserService ir_user;
	FRDService frd;
	FSService fs;
	GPUService gsp_gpu;
	LCDService gsp_lcd;
	LDRService ldr;
	MICService mic;
	NFCService nfc;
    NIMService nim;
	NDMService ndm;
	PTMService ptm;
	Y2RService y2r;

	// "srv:" commands
	void enableNotification(u32 messagePointer);
	void getServiceHandle(u32 messagePointer);
	void receiveNotification(u32 messagePointer);
	void registerClient(u32 messagePointer);
	void subscribe(u32 messagePointer);

  public:
	ServiceManager(std::span<u32, 16> regs, Memory& mem, GPU& gpu, u32& currentPID, Kernel& kernel);
	void reset();
	void initializeFS() { fs.initializeFilesystem(); }
	void handleSyncRequest(u32 messagePointer);

	// Forward a SendSyncRequest IPC message to the service with the respective handle
	void sendCommandToService(u32 messagePointer, Handle handle);

	// Wrappers for communicating with certain services
	void sendGPUInterrupt(GPUInterrupt type) { gsp_gpu.requestInterrupt(type); }
	void setGSPSharedMem(u8* ptr) { gsp_gpu.setSharedMem(ptr); }
	void setHIDSharedMem(u8* ptr) { hid.setSharedMem(ptr); }

	void signalDSPEvents() { dsp.signalEvents(); }

	// Input function wrappers
	void pressKey(u32 key) { hid.pressKey(key); }
	void releaseKey(u32 key) { hid.releaseKey(key); }
	s16 getCirclepadX() { return hid.getCirclepadX(); }
	s16 getCirclepadY() { return hid.getCirclepadY(); }
	void setCirclepadX(s16 x) { hid.setCirclepadX(x); }
	void setCirclepadY(s16 y) { hid.setCirclepadY(y); }
	void updateInputs(u64 currentTimestamp) { hid.updateInputs(currentTimestamp); }
	void setTouchScreenPress(u16 x, u16 y) { hid.setTouchScreenPress(x, y); }
	void releaseTouchScreen() { hid.releaseTouchScreen(); }
};
