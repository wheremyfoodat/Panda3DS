#include "services/apt.hpp"
#include "kernel.hpp"

namespace APTCommands {
	enum : u32 {
		GetLockHandle = 0x00010040,
		Initialize = 0x00020080,
		Enable = 0x00030040,
		InquireNotification = 0x000B0040,
		ReceiveParameter = 0x000D0080,
		ReplySleepQuery = 0x003E0080,
		NotifyToWait = 0x00430040,
		GetSharedFont = 0x00440000,
		GetWirelessRebootInfo = 0x00450040,
		AppletUtility = 0x004B00C2,
		SetApplicationCpuTimeLimit = 0x004F0080,
		GetApplicationCpuTimeLimit = 0x00500040,
		SetScreencapPostPermission = 0x00550040,
		CheckNew3DSApp = 0x01010000,
		CheckNew3DS = 0x01020000,
		TheSmashBrosFunction = 0x01030000
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
		Failure = 0xFFFFFFFF
	};
}

void APTService::reset() {
	// Set the default CPU time limit to 30%. Seems safe, as this is what Metroid 2 uses by default
	cpuTimeLimit = 30;

	// Reset the handles for the various service objects
	lockHandle = std::nullopt;
	notificationEvent = std::nullopt;
	resumeEvent = std::nullopt;
}

void APTService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case APTCommands::AppletUtility: appletUtility(messagePointer); break;
		case APTCommands::CheckNew3DS: checkNew3DS(messagePointer); break;
		case APTCommands::CheckNew3DSApp: checkNew3DSApp(messagePointer); break;
		case APTCommands::Enable: enable(messagePointer); break;
		case APTCommands::GetSharedFont: getSharedFont(messagePointer); break;
		case APTCommands::Initialize: initialize(messagePointer); break;
		case APTCommands::InquireNotification: [[likely]] inquireNotification(messagePointer); break;
		case APTCommands::GetApplicationCpuTimeLimit: getApplicationCpuTimeLimit(messagePointer); break;
		case APTCommands::GetLockHandle: getLockHandle(messagePointer); break;
		case APTCommands::GetWirelessRebootInfo: getWirelessRebootInfo(messagePointer); break;
		case APTCommands::NotifyToWait: notifyToWait(messagePointer); break;
		case APTCommands::ReceiveParameter: [[likely]] receiveParameter(messagePointer); break;
		case APTCommands::ReplySleepQuery: replySleepQuery(messagePointer); break;
		case APTCommands::SetApplicationCpuTimeLimit: setApplicationCpuTimeLimit(messagePointer); break;
		case APTCommands::SetScreencapPostPermission: setScreencapPostPermission(messagePointer); break;
		case APTCommands::TheSmashBrosFunction: theSmashBrosFunction(messagePointer); break;
		default: Helpers::panic("APT service requested. Command: %08X\n", command);
	}
}

void APTService::appletUtility(u32 messagePointer) {
	u32 utility = mem.read32(messagePointer + 4);
	u32 inputSize = mem.read32(messagePointer + 8);
	u32 outputSize = mem.read32(messagePointer + 12);
	u32 inputPointer = mem.read32(messagePointer + 20);

	log("APT::AppletUtility(utility = %d, input size = %x, output size = %x, inputPointer = %08X)\n", utility, inputSize,
		outputSize, inputPointer);
	mem.write32(messagePointer + 4, Result::Success);
}

void APTService::checkNew3DS(u32 messagePointer) {
	log("APT::CheckNew3DS\n");
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, (model == ConsoleModel::New3DS) ? 1 : 0); // u8, Status (0 = Old 3DS, 1 = New 3DS)
}

// TODO: Figure out the slight way this differs from APT::CheckNew3DS
void APTService::checkNew3DSApp(u32 messagePointer) {
	log("APT::CheckNew3DSApp\n");
	checkNew3DS(messagePointer);
}

void APTService::enable(u32 messagePointer) {
	log("APT::Enable\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void APTService::initialize(u32 messagePointer) {
	log("APT::Initialize\n");

	notificationEvent = kernel.makeEvent(ResetType::OneShot);
	resumeEvent = kernel.makeEvent(ResetType::OneShot);

	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0x04000000); // Translation descriptor
	mem.write32(messagePointer + 12, notificationEvent.value()); // Notification Event Handle
	mem.write32(messagePointer + 16, resumeEvent.value()); // Resume Event Handle
}

void APTService::inquireNotification(u32 messagePointer) {
	log("APT::InquireNotification (STUBBED TO FAIL)\n");

	// Thanks to our silly WaitSynchronization hacks, sometimes games will switch to the APT thread without actually getting a notif
	// After REing the APT code, I figured that making InquireNotification fail is one way of making games not crash when this happens
	// We should fix this in the future, when the sync object implementation is less hacky.
	mem.write32(messagePointer + 4, Result::Failure);
	mem.write32(messagePointer + 8, static_cast<u32>(NotificationType::None)); 
}

void APTService::getLockHandle(u32 messagePointer) {
	log("APT::GetLockHandle\n");

	// Create a lock handle if none exists
	if (!lockHandle.has_value() || kernel.getObject(lockHandle.value(), KernelObjectType::Mutex) == nullptr) {
		lockHandle = kernel.makeMutex();
	}

	mem.write32(messagePointer + 4, Result::Failure); // Result code
	mem.write32(messagePointer + 8, 0); // AppletAttr
	mem.write32(messagePointer + 12, 0); // APT State (bit0 = Power Button State, bit1 = Order To Close State)
	mem.write32(messagePointer + 16, 0); // Translation descriptor
	mem.write32(messagePointer + 20, lockHandle.value()); // Lock handle
}

// This apparently does nothing on the original kernel either?
void APTService::notifyToWait(u32 messagePointer) {
	log("APT::NotifyToWait\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void APTService::receiveParameter(u32 messagePointer) {
	const u32 app = mem.read32(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	log("APT::ReceiveParameter(app ID = %X, size = %04X) (STUBBED)\n", app, size);

	if (size > 0x1000) Helpers::panic("APT::ReceiveParameter with size > 0x1000");

	// TODO: Properly implement this. We currently stub it in the same way as 3dmoo
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0); // Sender App ID
	mem.write32(messagePointer + 12, 1); // Signal type (1 = app just started, 0xB = returning to app, 0xC = exiting app)
	mem.write32(messagePointer + 16, 0x10);
	mem.write32(messagePointer + 20, 0);
	mem.write32(messagePointer + 24, 0);
	mem.write32(messagePointer + 28, 0);
}

void APTService::replySleepQuery(u32 messagePointer) {
	log("APT::ReplySleepQuery (Stubbed)\n");
	mem.write32(messagePointer + 4, Result::Success);
}

void APTService::setApplicationCpuTimeLimit(u32 messagePointer) {
	u32 fixed = mem.read32(messagePointer + 4); // MUST be 1.
	u32 percentage = mem.read32(messagePointer + 8); // CPU time percentage between 5% and 89%
	log("APT::SetApplicationCpuTimeLimit (percentage = %d%%)\n", percentage);

	if (percentage < 5 || percentage > 89 || fixed != 1) {
		Helpers::panic("Invalid parameters passed to APT::SetApplicationCpuTimeLimit");
	} else {
		mem.write32(messagePointer + 4, Result::Success);
		cpuTimeLimit = percentage;
	}
}

void APTService::getApplicationCpuTimeLimit(u32 messagePointer) {
	log("APT::GetApplicationCpuTimeLimit\n");
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, cpuTimeLimit);
}

void APTService::setScreencapPostPermission(u32 messagePointer) {
	u32 perm = mem.read32(messagePointer + 4);
	log("APT::SetScreencapPostPermission (perm = %d)\n");

	// Apparently only 1-3 are valid values, but I see 0 used in some games like Pokemon Rumble
	mem.write32(messagePointer + 4, Result::Success);
	screencapPostPermission = perm;
}

void APTService::getSharedFont(u32 messagePointer) {
	log("APT::GetSharedFont\n");

	constexpr u32 fontVaddr = 0x18000000;
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, fontVaddr);
	mem.write32(messagePointer + 16, KernelHandles::FontSharedMemHandle);
}

// This function is entirely undocumented. We know Smash Bros uses it and that it normally writes 2 to cmdreply[2] on New 3DS
// And that writing 1 stops it from accessing the ir:USER service for New 3DS HID use
void APTService::theSmashBrosFunction(u32 messagePointer) {
	log("APT: Called the elusive Smash Bros function\n");

	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, (model == ConsoleModel::New3DS) ? 2 : 1);
}

void APTService::getWirelessRebootInfo(u32 messagePointer) {
	const u32 size = mem.read32(messagePointer + 4); // Size of data to read
	log("APT::GetWirelessRebootInfo (size = %X)\n", size);

	if (size > 0x10)
		Helpers::panic("APT::GetWirelessInfo with size > 0x10 bytes");

	mem.write32(messagePointer + 4, Result::Success);
	for (u32 i = 0; i < size; i++) {
		mem.write8(messagePointer + 0x104 + i, 0); // Temporarily stub this until we add SetWirelessRebootInfo
	}
}