#pragma once
#include <optional>
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"

#include "applets/applet_manager.hpp"

// Yay, more circular dependencies
class Kernel;

enum class ConsoleModel : u32 {
	Old3DS, New3DS
};

// https://www.3dbrew.org/wiki/NS_and_APT_Services#Command
namespace APT::Transitions {
	enum : u32 {
		None = 0,
		Wakeup = 1,
		Request = 2,
		Response = 3,
		Exit = 4,
		Message = 5,
		HomeButtonSingle = 6,
		HomeButtonDouble = 7,
		DSPSleep = 8,
		DSPWakeup = 9,
		WakeupByExit = 10,
		WakuepByPause = 11,
		WakeupByCancel = 12,
		WakeupByCancelAll = 13,
		WakeupByPowerButton = 14,
		WakeupToJumpHome = 15,
		RequestForApplet = 16,
		WakeupToLaunchApp = 17,
		ProcessDed = 0x41
	};
}

class APTService {
	Handle handle = KernelHandles::APT;
	Memory& mem;
	Kernel& kernel;

	std::optional<Handle> lockHandle = std::nullopt;
	std::optional<Handle> notificationEvent = std::nullopt;
	std::optional<Handle> resumeEvent = std::nullopt;

	ConsoleModel model = ConsoleModel::Old3DS;
	Applets::AppletManager appletManager;

	MAKE_LOG_FUNCTION(log, aptLogger)

	// Service commands
	void appletUtility(u32 messagePointer);
	void getApplicationCpuTimeLimit(u32 messagePointer);
	void getLockHandle(u32 messagePointer);
	void checkNew3DS(u32 messagePointer);
	void checkNew3DSApp(u32 messagePointer);
	void enable(u32 messagePointer);
	void getAppletInfo(u32 messagePointer);
	void getCaptureInfo(u32 messagePointer);
	void getSharedFont(u32 messagePointer);
	void getWirelessRebootInfo(u32 messagePointer);
	void glanceParameter(u32 messagePointer);
	void initialize(u32 messagePointer);
	void inquireNotification(u32 messagePointer);
	void isRegistered(u32 messagePointer);
	void loadSysMenuArg(u32 messagePointer);
	void notifyToWait(u32 messagePointer);
	void preloadLibraryApplet(u32 messagePointer);
	void prepareToStartLibraryApplet(u32 messagePointer);
	void receiveDeliverArg(u32 messagePointer);
	void receiveParameter(u32 messagePointer);
	void replySleepQuery(u32 messagePointer);
	void setApplicationCpuTimeLimit(u32 messagePointer);
	void setScreencapPostPermission(u32 messagePointer);
	void sendParameter(u32 messagePointer);
	void startLibraryApplet(u32 messagePointer);
	void theSmashBrosFunction(u32 messagePointer);

	// Percentage of the syscore available to the application, between 5% and 89%
	u32 cpuTimeLimit;

	enum class NotificationType : u32 {
		None = 0,
		HomeButton1 = 1,
		HomeButton2 = 2,
		SleepQuery = 3,
		SleepCanceledByOpen = 4,
		SleepAccepted = 5,
		SleepAwake = 6,
		Shutdown = 7,
		PowerButtonClick = 8,
		PowerButtonClear = 9,
		TrySleep = 10,
		OrderToClose = 11
	};

	u32 screencapPostPermission;

public:
	APTService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel), appletManager(mem) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};