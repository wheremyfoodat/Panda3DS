#include "services/apt.hpp"
#include "kernel.hpp"

namespace APTCommands {
	enum : u32 {
		GetLockHandle = 0x00010040,
		Enable = 0x00030040,
		ReceiveParameter = 0x000D0080,
		ReplySleepQuery = 0x003E0080,
		NotifyToWait = 0x00430040,
		AppletUtility = 0x004B00C2,
		SetApplicationCpuTimeLimit = 0x004F0080,
		GetApplicationCpuTimeLimit = 0x00500040,
		CheckNew3DSApp = 0x01010000,
		CheckNew3DS = 0x01020000
	};
}

namespace Model {
	enum : u8 {
		Old3DS = 0,
		New3DS = 1
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

	lockHandle = std::nullopt;
}

void APTService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case APTCommands::AppletUtility: appletUtility(messagePointer); break;
		case APTCommands::CheckNew3DS: checkNew3DS(messagePointer); break;
		case APTCommands::CheckNew3DSApp: checkNew3DSApp(messagePointer); break;
		case APTCommands::Enable: enable(messagePointer); break;
		case APTCommands::GetApplicationCpuTimeLimit: getApplicationCpuTimeLimit(messagePointer); break;
		case APTCommands::GetLockHandle: getLockHandle(messagePointer); break;
		case APTCommands::NotifyToWait: notifyToWait(messagePointer); break;
		case APTCommands::ReceiveParameter: receiveParameter(messagePointer); break;
		case APTCommands::ReplySleepQuery: replySleepQuery(messagePointer); break;
		case APTCommands::SetApplicationCpuTimeLimit: setApplicationCpuTimeLimit(messagePointer); break;
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
	mem.write8(messagePointer + 8, Model::Old3DS); // u8, Status (0 = Old 3DS, 1 = New 3DS)
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

void APTService::getLockHandle(u32 messagePointer) {
	log("APT::GetLockHandle\n");

	// Create a lock handle if none exists
	if (!lockHandle.has_value() || kernel.getObject(lockHandle.value(), KernelObjectType::Mutex) == nullptr) {
		lockHandle = kernel.makeMutex();
	}

	mem.write32(messagePointer + 4, Result::Success); // Result code
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