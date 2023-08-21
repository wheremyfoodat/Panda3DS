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
#include "services/http.hpp"
#include "services/ir_user.hpp"
#include "services/ldr_ro.hpp"
#include "services/mcu/mcu_hwc.hpp"
#include "services/mic.hpp"
#include "services/ndm.hpp"
#include "services/news_u.hpp"
#include "services/nfc.hpp"
#include "services/nim.hpp"
#include "services/ptm.hpp"
#include "services/soc.hpp"
#include "services/ssl.hpp"
#include "services/y2r.hpp"

struct EmulatorConfig;
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
	HTTPService http;
	IRUserService ir_user;
	FRDService frd;
	FSService fs;
	GPUService gsp_gpu;
	LCDService gsp_lcd;
	LDRService ldr;
	MICService mic;
	NDMService ndm;
	NewsUService news_u;
	NFCService nfc;
    NIMService nim;
	PTMService ptm;
	SOCService soc;
	SSLService ssl;
	Y2RService y2r;

	MCU::HWCService mcu_hwc;

	// "srv:" commands
	void enableNotification(u32 messagePointer);
	void getServiceHandle(u32 messagePointer);
	void receiveNotification(u32 messagePointer);
	void registerClient(u32 messagePointer);
	void subscribe(u32 messagePointer);

  public:
	ServiceManager(std::span<u32, 16> regs, Memory& mem, GPU& gpu, u32& currentPID, Kernel& kernel, const EmulatorConfig& config);
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
	HIDService& getHID() { return hid; }
};
