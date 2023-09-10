#include "services/ndm.hpp"
#include "ipc.hpp"

namespace NDMCommands {
	enum : u32 {
		EnterExclusiveState = 0x00010042,
		ExitExclusiveState = 0x00020002,	
		OverrideDefaultDaemons = 0x00140040,
		SuspendDaemons = 0x00060040,
		ResumeDaemons = 0x00070040,
		SuspendScheduler = 0x00080040,
		ResumeScheduler = 0x00090000,
		ClearHalfAwakeMacFilter = 0x00170000,
	};
}

void NDMService::reset() {}

void NDMService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case NDMCommands::EnterExclusiveState: enterExclusiveState(messagePointer); break;
		case NDMCommands::ExitExclusiveState: exitExclusiveState(messagePointer); break;
		case NDMCommands::ClearHalfAwakeMacFilter: clearHalfAwakeMacFilter(messagePointer); break;
		case NDMCommands::OverrideDefaultDaemons: overrideDefaultDaemons(messagePointer); break;
		case NDMCommands::ResumeDaemons: resumeDaemons(messagePointer); break;
		case NDMCommands::ResumeScheduler: resumeScheduler(messagePointer); break;
		case NDMCommands::SuspendDaemons: suspendDaemons(messagePointer); break;
		case NDMCommands::SuspendScheduler: suspendScheduler(messagePointer); break;
		default: Helpers::panic("NDM service requested. Command: %08X\n", command);
	}
}

void NDMService::enterExclusiveState(u32 messagePointer) {
	log("NDM::EnterExclusiveState (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NDMService::exitExclusiveState(u32 messagePointer) {
	log("NDM::ExitExclusiveState (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x2, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NDMService::overrideDefaultDaemons(u32 messagePointer) {
	log("NDM::OverrideDefaultDaemons (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x14, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NDMService::resumeDaemons(u32 messagePointer) {
	log("NDM::resumeDaemons (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x7, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NDMService::suspendDaemons(u32 messagePointer) {
	log("NDM::SuspendDaemons (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x6, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NDMService::resumeScheduler(u32 messagePointer) {
	log("NDM::ResumeScheduler (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x9, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NDMService::suspendScheduler(u32 messagePointer) {
	log("NDM::SuspendScheduler (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x8, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NDMService::clearHalfAwakeMacFilter(u32 messagePointer) {
	log("NDM::ClearHalfAwakeMacFilter (stubbed)\n");
	mem.write32(messagePointer, IPC::responseHeader(0x17, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}