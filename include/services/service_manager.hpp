#pragma once
#include <array>
#include <optional>
#include <span>
#include <string>
#include <unordered_set>

#include "kernel_types.hpp"
#include "logger.hpp"
#include "lua_manager.hpp"
#include "memory.hpp"
#include "services/ac.hpp"
#include "services/act.hpp"
#include "services/am.hpp"
#include "services/apt.hpp"
#include "services/boss.hpp"
#include "services/cam.hpp"
#include "services/cecd.hpp"
#include "services/cfg.hpp"
#include "services/csnd.hpp"
#include "services/dlp_srvr.hpp"
#include "services/dsp.hpp"
#include "services/frd.hpp"
#include "services/fs.hpp"
#include "services/gsp_gpu.hpp"
#include "services/gsp_lcd.hpp"
#include "services/hid.hpp"
#include "services/http.hpp"
#include "services/ir/ir_user.hpp"
#include "services/ldr_ro.hpp"
#include "services/mcu/mcu_hwc.hpp"
#include "services/mic.hpp"
#include "services/ndm.hpp"
#include "services/news_u.hpp"
#include "services/nfc.hpp"
#include "services/nim.hpp"
#include "services/ns.hpp"
#include "services/nwm_uds.hpp"
#include "services/ptm.hpp"
#include "services/service_intercept.hpp"
#include "services/soc.hpp"
#include "services/ssl.hpp"
#include "services/y2r.hpp"

struct EmulatorConfig;
// More circular dependencies!!
class Kernel;

class ServiceManager {
	using Handle = HorizonHandle;

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
	CSNDService csnd;
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
	NwmUdsService nwm_uds;
	NIMService nim;
	NSService ns;
	PTMService ptm;
	SOCService soc;
	SSLService ssl;
	Y2RService y2r;

	MCU::HWCService mcu_hwc;

	// We allow Lua scripts to intercept service calls and allow their own code to be ran on SyncRequests
	// For example, if we want to intercept dsp::DSP ReadPipe (Header: 0x000E00C0), the "serviceName" field would be "dsp::DSP"
	// and the "function" field would be 0x000E00C0
	LuaManager& lua;
	std::unordered_set<InterceptedService> interceptedServices = {};
	// Calling std::unordered_set<T>::size() compiles to a fairly non-trivial function call on Clang, so we store this
	// separately and check it on service calls, for performance reasons
	bool haveServiceIntercepts = false;

	// Checks for whether a service call is intercepted by Lua and handles it. Returns true if Lua told us not to handle the function,
	// or false if we should handle it as normal
	bool checkForIntercept(u32 messagePointer, Handle handle);

	// "srv:" commands
	void enableNotification(u32 messagePointer);
	void getServiceHandle(u32 messagePointer);
	void publishToSubscriber(u32 messagePointer);
	void receiveNotification(u32 messagePointer);
	void registerClient(u32 messagePointer);
	void subscribe(u32 messagePointer);
	void unsubscribe(u32 messagePointer);

  public:
	ServiceManager(std::span<u32, 16> regs, Memory& mem, GPU& gpu, u32& currentPID, Kernel& kernel, const EmulatorConfig& config, LuaManager& lua);
	void reset();
	void initializeFS() { fs.initializeFilesystem(); }
	void handleSyncRequest(u32 messagePointer);

	// Forward a SendSyncRequest IPC message to the service with the respective handle
	void sendCommandToService(u32 messagePointer, Handle handle);

	// Wrappers for communicating with certain services
	void sendGPUInterrupt(GPUInterrupt type) { gsp_gpu.requestInterrupt(type); }
	void setGSPSharedMem(u8* ptr) { gsp_gpu.setSharedMem(ptr); }
	void setHIDSharedMem(u8* ptr) { hid.setSharedMem(ptr); }
	void setCSNDSharedMem(u8* ptr) { csnd.setSharedMemory(ptr); }

	// Input function wrappers
	HIDService& getHID() { return hid; }
	NFCService& getNFC() { return nfc; }
	DSPService& getDSP() { return dsp; }
	Y2RService& getY2R() { return y2r; }
	IRUserService& getIRUser() { return ir_user; }

	void addServiceIntercept(const std::string& service, u32 function) {
		interceptedServices.insert(InterceptedService(service, function));
		haveServiceIntercepts = true;
	}

	void clearServiceIntercepts() {
		interceptedServices.clear();
		haveServiceIntercepts = false;
	}
};
