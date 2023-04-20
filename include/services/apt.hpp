#pragma once
#include <optional>
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"

// Yay, more circular dependencies
class Kernel;

enum class ConsoleModel : u32 {
	Old3DS, New3DS
};

class APTService {
	Handle handle = KernelHandles::APT;
	Memory& mem;
	Kernel& kernel;

	std::optional<Handle> lockHandle = std::nullopt;
	std::optional<Handle> notificationEvent = std::nullopt;
	std::optional<Handle> resumeEvent = std::nullopt;

	ConsoleModel model = ConsoleModel::Old3DS;

	MAKE_LOG_FUNCTION(log, aptLogger)

	// Service commands
	void appletUtility(u32 messagePointer);
	void getApplicationCpuTimeLimit(u32 messagePointer);
	void getLockHandle(u32 messagePointer);
	void checkNew3DS(u32 messagePointer);
	void checkNew3DSApp(u32 messagePointer);
	void enable(u32 messagePointer);
	void getSharedFont(u32 messagePointer);
	void getWirelessRebootInfo(u32 messagePointer);
	void glanceParameter(u32 messagePointer);
	void initialize(u32 messagePointer);
	void inquireNotification(u32 messagePointer);
	void notifyToWait(u32 messagePointer);
	void receiveParameter(u32 messagePointer);
	void replySleepQuery(u32 messagePointer);
	void setApplicationCpuTimeLimit(u32 messagePointer);
	void setScreencapPostPermission(u32 messagePointer);
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
	APTService(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {}
	void reset();
	void handleSyncRequest(u32 messagePointer);
};