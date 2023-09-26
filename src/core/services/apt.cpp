#include "services/apt.hpp"
#include "ipc.hpp"
#include "kernel.hpp"

#include <vector>

namespace APTCommands {
	enum : u32 {
		GetLockHandle = 0x00010040,
		Initialize = 0x00020080,
		Enable = 0x00030040,
		GetAppletInfo = 0x00060040,
		IsRegistered = 0x00090040,
		InquireNotification = 0x000B0040,
		SendParameter = 0x000C0104,
		ReceiveParameter = 0x000D0080,
		GlanceParameter = 0x000E0080,
		PreloadLibraryApplet = 0x00160040,
		PrepareToStartLibraryApplet = 0x00180040,
		StartLibraryApplet = 0x001E0084,
		ReceiveDeliverArg = 0x00350080,
		LoadSysMenuArg = 0x00360040,
		ReplySleepQuery = 0x003E0080,
		NotifyToWait = 0x00430040,
		GetSharedFont = 0x00440000,
		GetWirelessRebootInfo = 0x00450040,
		GetCaptureInfo = 0x004A0040,
		AppletUtility = 0x004B00C2,
		SetApplicationCpuTimeLimit = 0x004F0080,
		GetApplicationCpuTimeLimit = 0x00500040,
		SetScreencapPostPermission = 0x00550040,
		CheckNew3DSApp = 0x01010000,
		CheckNew3DS = 0x01020000,
		TheSmashBrosFunction = 0x01030000
	};
}

// https://www.3dbrew.org/wiki/NS_and_APT_Services#Command
namespace APTTransitions {
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

void APTService::reset() {
	// Set the default CPU time limit to 30%. Seems safe, as this is what Metroid 2 uses by default
	cpuTimeLimit = 30;

	// Reset the handles for the various service objects
	lockHandle = std::nullopt;
	notificationEvent = std::nullopt;
	resumeEvent = std::nullopt;

	appletManager.reset();
}

void APTService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case APTCommands::AppletUtility: appletUtility(messagePointer); break;
		case APTCommands::CheckNew3DS: checkNew3DS(messagePointer); break;
		case APTCommands::CheckNew3DSApp: checkNew3DSApp(messagePointer); break;
		case APTCommands::Enable: enable(messagePointer); break;
		case APTCommands::GetAppletInfo: getAppletInfo(messagePointer); break;
		case APTCommands::GetSharedFont: getSharedFont(messagePointer); break;
		case APTCommands::Initialize: initialize(messagePointer); break;
		case APTCommands::InquireNotification: [[likely]] inquireNotification(messagePointer); break;
		case APTCommands::IsRegistered: isRegistered(messagePointer); break;
		case APTCommands::GetApplicationCpuTimeLimit: getApplicationCpuTimeLimit(messagePointer); break;
		case APTCommands::GetCaptureInfo: getCaptureInfo(messagePointer); break;
		case APTCommands::GetLockHandle: getLockHandle(messagePointer); break;
		case APTCommands::GetWirelessRebootInfo: getWirelessRebootInfo(messagePointer); break;
		case APTCommands::GlanceParameter: glanceParameter(messagePointer); break;
		case APTCommands::LoadSysMenuArg: loadSysMenuArg(messagePointer); break;
		case APTCommands::NotifyToWait: notifyToWait(messagePointer); break;
		case APTCommands::PreloadLibraryApplet: preloadLibraryApplet(messagePointer); break;
		case APTCommands::PrepareToStartLibraryApplet: prepareToStartLibraryApplet(messagePointer); break;
		case APTCommands::ReceiveDeliverArg: receiveDeliverArg(messagePointer); break;
		case APTCommands::ReceiveParameter: [[likely]] receiveParameter(messagePointer); break;
		case APTCommands::ReplySleepQuery: replySleepQuery(messagePointer); break;
		case APTCommands::SetApplicationCpuTimeLimit: setApplicationCpuTimeLimit(messagePointer); break;
		case APTCommands::SendParameter: sendParameter(messagePointer); break;
		case APTCommands::SetScreencapPostPermission: setScreencapPostPermission(messagePointer); break;
		case APTCommands::TheSmashBrosFunction: theSmashBrosFunction(messagePointer); break;
		default:
			Helpers::panicDev("APT service requested. Command: %08X\n", command);
			mem.write32(messagePointer + 4, Result::Success);
			break;
	}
}

void APTService::appletUtility(u32 messagePointer) {
	u32 utility = mem.read32(messagePointer + 4);
	u32 inputSize = mem.read32(messagePointer + 8);
	u32 outputSize = mem.read32(messagePointer + 12);
	u32 inputPointer = mem.read32(messagePointer + 20);

	log("APT::AppletUtility(utility = %d, input size = %x, output size = %x, inputPointer = %08X) (Stubbed)\n", utility, inputSize, outputSize,
		inputPointer);

	std::vector<u8> out(outputSize);
	const u32 outputBuffer = mem.read32(messagePointer + 0x104);

	if (outputSize >= 1 && utility == 6) {
		// TryLockTransition expects a bool indicating success in the output buffer. Set it to true to avoid games panicking (Thanks to Citra)
		out[0] = true;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x4B, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, Result::Success);

	for (u32 i = 0; i < outputSize; i++) {
		mem.write8(outputBuffer + i, out[i]);
	}
}

void APTService::getAppletInfo(u32 messagePointer) {
	const u32 appID = mem.read32(messagePointer + 4);
	Helpers::warn("APT::GetAppletInfo (appID = %X)\n", appID);

	mem.write32(messagePointer, IPC::responseHeader(0x06, 7, 0));
	mem.write32(messagePointer + 4, Result::Success);

	mem.write8(messagePointer + 20, 1); // 1 = registered
	mem.write8(messagePointer + 24, 1); // 1 = loaded
	// TODO: The rest of this
}

void APTService::isRegistered(u32 messagePointer) {
	const u32 appID = mem.read32(messagePointer + 4);
	Helpers::warn("APT::IsRegistered (appID = %X)", appID);

	mem.write32(messagePointer, IPC::responseHeader(0x09, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, 1); // Return that the app is always registered. This might break with home menu?
}

void APTService::preloadLibraryApplet(u32 messagePointer) {
	const u32 appID = mem.read32(messagePointer + 4);
	log("APT::PreloadLibraryApplet (app ID = %X) (stubbed)\n", appID);

	mem.write32(messagePointer, IPC::responseHeader(0x16, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void APTService::prepareToStartLibraryApplet(u32 messagePointer) {
	const u32 appID = mem.read32(messagePointer + 4);
	log("APT::PrepareToStartLibraryApplet (app ID = %X) (stubbed)\n", appID);

	mem.write32(messagePointer, IPC::responseHeader(0x16, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void APTService::checkNew3DS(u32 messagePointer) {
	log("APT::CheckNew3DS\n");
	mem.write32(messagePointer, IPC::responseHeader(0x102, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, (model == ConsoleModel::New3DS) ? 1 : 0); // u8, Status (0 = Old 3DS, 1 = New 3DS)
}

// TODO: Figure out the slight way this differs from APT::CheckNew3DS
void APTService::checkNew3DSApp(u32 messagePointer) {
	log("APT::CheckNew3DSApp\n");
	mem.write32(messagePointer, IPC::responseHeader(0x101, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, (model == ConsoleModel::New3DS) ? 1 : 0); // u8, Status (0 = Old 3DS, 1 = New 3DS)
}

void APTService::enable(u32 messagePointer) {
	log("APT::Enable\n");
	mem.write32(messagePointer, IPC::responseHeader(0x3, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);

	// Some apps like Home Menu and Game Notes seem to rely on resume event being triggered here. Doesn't seem to break anything, so
	if (resumeEvent.has_value()) {
		kernel.signalEvent(resumeEvent.value());
	}
}

void APTService::initialize(u32 messagePointer) {
	log("APT::Initialize\n");

	if (!notificationEvent.has_value() || !resumeEvent.has_value()) {
		notificationEvent = kernel.makeEvent(ResetType::OneShot);
		resumeEvent = kernel.makeEvent(ResetType::OneShot);

		kernel.signalEvent(resumeEvent.value()); // Seems to be signalled on startup
	}

	mem.write32(messagePointer, IPC::responseHeader(0x2, 1, 3));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0x04000000); // Translation descriptor
	mem.write32(messagePointer + 12, notificationEvent.value()); // Notification Event Handle
	mem.write32(messagePointer + 16, resumeEvent.value()); // Resume Event Handle
}

void APTService::inquireNotification(u32 messagePointer) {
	log("APT::InquireNotification (STUBBED TO RETURN NONE)\n");

	mem.write32(messagePointer, IPC::responseHeader(0xB, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, static_cast<u32>(NotificationType::None));
}

void APTService::getLockHandle(u32 messagePointer) {
	log("APT::GetLockHandle\n");

	// Create a lock handle if none exists
	if (!lockHandle.has_value() || kernel.getObject(lockHandle.value(), KernelObjectType::Mutex) == nullptr) {
		lockHandle = kernel.makeMutex();
	}

	mem.write32(messagePointer, IPC::responseHeader(0x1, 3, 2));
	mem.write32(messagePointer + 4, Result::Success); // Result code
	mem.write32(messagePointer + 8, 0); // AppletAttr
	mem.write32(messagePointer + 12, 0); // APT State (bit0 = Power Button State, bit1 = Order To Close State)
	mem.write32(messagePointer + 16, 0); // Translation descriptor
	mem.write32(messagePointer + 20, lockHandle.value()); // Lock handle
}

// This apparently does nothing on the original kernel either?
void APTService::notifyToWait(u32 messagePointer) {
	log("APT::NotifyToWait\n");
	mem.write32(messagePointer, IPC::responseHeader(0x43, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void APTService::sendParameter(u32 messagePointer) {
	const u32 sourceAppID = mem.read32(messagePointer + 4);
	const u32 destAppID = mem.read32(messagePointer + 8);
	const u32 cmd = mem.read32(messagePointer + 12);
	const u32 paramSize = mem.read32(messagePointer + 16);

	const u32 parameterHandle = mem.read32(messagePointer + 24); // What dis?
	const u32 parameterPointer = mem.read32(messagePointer + 32);
	log("APT::SendParameter (source app = %X, dest app = %X, cmd = %X, size = %X) (Stubbed)", sourceAppID, destAppID, cmd, paramSize);

	mem.write32(messagePointer, IPC::responseHeader(0x0C, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);

	if (sourceAppID != Applets::AppletIDs::Application) {
		Helpers::warn("APT::SendParameter: Unimplemented source applet ID");
	}

	Applets::AppletBase* destApplet = appletManager.getApplet(destAppID);
	if (destApplet == nullptr) {
		Helpers::warn("APT::SendParameter: Unimplemented dest applet ID");
	} else {
		auto result = destApplet->receiveParameter();
	}

	if (resumeEvent.has_value()) {
		kernel.signalEvent(resumeEvent.value());
	}
}

void APTService::receiveParameter(u32 messagePointer) {
	const u32 app = mem.read32(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	log("APT::ReceiveParameter(app ID = %X, size = %04X) (STUBBED)\n", app, size);

	if (size > 0x1000) Helpers::panic("APT::ReceiveParameter with size > 0x1000");

	// TODO: Properly implement this. We currently stub somewhat like 3dmoo
	mem.write32(messagePointer, IPC::responseHeader(0xD, 4, 4));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0); // Sender App ID
	mem.write32(messagePointer + 12, APTTransitions::Wakeup); // Command
	mem.write32(messagePointer + 16, 0);
	mem.write32(messagePointer + 20, 0x10);
	mem.write32(messagePointer + 24, 0);
	mem.write32(messagePointer + 28, 0);
}

void APTService::glanceParameter(u32 messagePointer) {
	const u32 app = mem.read32(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	log("APT::GlanceParameter(app ID = %X, size = %04X) (STUBBED)\n", app, size);

	if (size > 0x1000) Helpers::panic("APT::GlanceParameter with size > 0x1000");

	// TODO: Properly implement this. We currently stub it similar
	mem.write32(messagePointer, IPC::responseHeader(0xE, 4, 4));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0); // Sender App ID
	mem.write32(messagePointer + 12, APTTransitions::Wakeup); // Command
	mem.write32(messagePointer + 16, 0);
	mem.write32(messagePointer + 20, 0);
	mem.write32(messagePointer + 24, 0);
	mem.write32(messagePointer + 28, 0);
}

void APTService::replySleepQuery(u32 messagePointer) {
	log("APT::ReplySleepQuery (Stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x3E, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void APTService::setApplicationCpuTimeLimit(u32 messagePointer) {
	u32 fixed = mem.read32(messagePointer + 4); // MUST be 1.
	u32 percentage = mem.read32(messagePointer + 8); // CPU time percentage between 5% and 89%
	log("APT::SetApplicationCpuTimeLimit (percentage = %d%%)\n", percentage);

	if (percentage < 5 || percentage > 89 || fixed != 1) {
		Helpers::panic("Invalid parameters passed to APT::SetApplicationCpuTimeLimit");
	} else {
		mem.write32(messagePointer, IPC::responseHeader(0x4F, 1, 0));
		mem.write32(messagePointer + 4, Result::Success);
		cpuTimeLimit = percentage;
	}
}

void APTService::getApplicationCpuTimeLimit(u32 messagePointer) {
	log("APT::GetApplicationCpuTimeLimit\n");
	mem.write32(messagePointer, IPC::responseHeader(0x50, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, cpuTimeLimit);
}

void APTService::setScreencapPostPermission(u32 messagePointer) {
	u32 perm = mem.read32(messagePointer + 4);
	log("APT::SetScreencapPostPermission (perm = %d)\n", perm);

	mem.write32(messagePointer, IPC::responseHeader(0x55, 1, 0));
	// Apparently only 1-3 are valid values, but I see 0 used in some games like Pokemon Rumble
	mem.write32(messagePointer + 4, Result::Success);
	screencapPostPermission = perm;
}

void APTService::getSharedFont(u32 messagePointer) {
	log("APT::GetSharedFont\n");

	constexpr u32 fontVaddr = 0x18000000;
	mem.write32(messagePointer, IPC::responseHeader(0x44, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, fontVaddr);
	mem.write32(messagePointer + 16, KernelHandles::FontSharedMemHandle);
}

// This function is entirely undocumented. We know Smash Bros uses it and that it normally writes 2 to cmdreply[2] on New 3DS
// And that writing 1 stops it from accessing the ir:USER service for New 3DS HID use
void APTService::theSmashBrosFunction(u32 messagePointer) {
	log("APT: Called the elusive Smash Bros function\n");

	mem.write32(messagePointer, IPC::responseHeader(0x103, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, (model == ConsoleModel::New3DS) ? 2 : 1);
}

void APTService::getWirelessRebootInfo(u32 messagePointer) {
	const u32 size = mem.read32(messagePointer + 4); // Size of data to read
	log("APT::GetWirelessRebootInfo (size = %X)\n", size);

	if (size > 0x10)
		Helpers::panic("APT::GetWirelessInfo with size > 0x10 bytes");

	mem.write32(messagePointer, IPC::responseHeader(0x45, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	for (u32 i = 0; i < size; i++) {
		mem.write8(messagePointer + 0x104 + i, 0); // Temporarily stub this until we add SetWirelessRebootInfo
	}
}

void APTService::receiveDeliverArg(u32 messagePointer) {
	const u32 parameterSize = mem.read32(messagePointer + 4);
	const u32 hmacSize = mem.read32(messagePointer + 8);
	const u32 parameter = mem.read32(messagePointer + 0x104);
	const u32 hmac = mem.read32(messagePointer + 0x10C);
	log("APT::ReceiveDeliverArg (parameter size = %X, HMAC size = %X, parameter pointer = %X, HMAC pointer = %X) (stubbed)\n", parameterSize, hmacSize, parameter, hmac);

	mem.write32(messagePointer, IPC::responseHeader(0x35, 4, 4));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0); // Program ID
	mem.write8(messagePointer + 16, 1); // Is valid response
	mem.write32(messagePointer + 20, IPC::pointerHeader(0, sizeof(u32) * parameterSize, IPC::BufferType::Send));
	mem.write32(messagePointer + 24, 0xDEADBEEF); // TODO: look into how this works if program reads from this address
	mem.write32(messagePointer + 28, IPC::pointerHeader(1, sizeof(u32) * hmacSize, IPC::BufferType::Send));
	mem.write32(messagePointer + 32, 0xDEADBEEF);
}

void APTService::loadSysMenuArg(u32 messagePointer) {
	const u32 outputSize = mem.read32(messagePointer + 4);
	log("APT::LoadSysMenuArg (output size = %X) (stubbed)\n", outputSize);

	mem.write32(messagePointer, IPC::responseHeader(0x35, 4, 4));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, IPC::pointerHeader(0, sizeof(u32) * outputSize, IPC::BufferType::Send));
	mem.write32(messagePointer + 12, 0xDEADBEEF);
}

void APTService::getCaptureInfo(u32 messagePointer) {
	const u32 size = mem.read32(messagePointer + 4);
	const u32 captureBufferInfo = mem.read32(messagePointer + 0x104);
	log("APT::GetCaptureInfo (size = %X, capture buffer info pointer = %X) (Stubbed)\n", size, captureBufferInfo);

	mem.write32(messagePointer, IPC::responseHeader(0x4A, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, IPC::pointerHeader(0, sizeof(u32) * size, IPC::BufferType::Send));
	mem.write32(messagePointer + 12, 0xDEADDEAD);
}