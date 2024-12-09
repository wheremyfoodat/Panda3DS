#include "services/nim.hpp"

#include "ipc.hpp"
#include "kernel.hpp"

namespace NIMCommands {
	enum : u32 {
		GetBackgroundEventForMenu = 0x00050000,
		GetTaskInfos = 0x000F0042,
		IsPendingAutoTitleDownloadTasks = 0x00150000,
		GetAutoTitleDownloadTaskInfos = 0x00170042,
		Initialize = 0x00210000,
	};
}

void NIMService::reset() { backgroundSystemUpdateEvent = std::nullopt; }

void NIMService::handleSyncRequest(u32 messagePointer, Type type) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		default:
			if (type == Type::AOC) {
				switch (command) {
					case NIMCommands::Initialize: initialize(messagePointer); break;

					default: Helpers::panic("NIM AOC service requested. Command: %08X\n", command);
				}
			} else if (type == Type::U) {
				switch (command) {
					case NIMCommands::GetAutoTitleDownloadTaskInfos: getAutoTitleDownloadTaskInfos(messagePointer); break;
					case NIMCommands::GetBackgroundEventForMenu: getBackgroundEventForMenu(messagePointer); break;
					case NIMCommands::GetTaskInfos: getTaskInfos(messagePointer); break;
					case NIMCommands::IsPendingAutoTitleDownloadTasks: isPendingAutoTitleDownloadTasks(messagePointer); break;

					default: Helpers::panic("NIM U service requested. Command: %08X\n", command);
				}
			} else {
				Helpers::panic("NIM service requested. Command: %08X\n", command);
			}
	}
}

void NIMService::initialize(u32 messagePointer) {
	log("NIM::Initialize\n");
	mem.write32(messagePointer, IPC::responseHeader(0x21, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NIMService::getTaskInfos(u32 messagePointer) {
	const u32 numTaskInfos = mem.read32(messagePointer + 4);
	const u32 taskInfos = mem.read32(messagePointer + 12);
	log("NIM::GetTaskInfos (num task infos = %X, task infos pointer = %X) (stubbed)\n", numTaskInfos, taskInfos);

	mem.write32(messagePointer, IPC::responseHeader(0xF, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);  // Read task infos?
	mem.write32(messagePointer + 12, IPC::pointerHeader(0, 0, IPC::BufferType::Receive));
	mem.write32(messagePointer + 16, taskInfos);
}

void NIMService::getAutoTitleDownloadTaskInfos(u32 messagePointer) {
	const u32 numTaskInfos = mem.read32(messagePointer + 4);
	const u32 taskInfos = mem.read32(messagePointer + 12);
	log("NIM::GetAutoTitleDownloadTaskInfos (num task infos = %X, task infos pointer = %X) (stubbed)\n", numTaskInfos, taskInfos);

	mem.write32(messagePointer, IPC::responseHeader(0x17, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0);  // Read task infos?
	mem.write32(messagePointer + 12, IPC::pointerHeader(0, 0, IPC::BufferType::Receive));
	mem.write32(messagePointer + 16, taskInfos);
}

void NIMService::isPendingAutoTitleDownloadTasks(u32 messagePointer) {
	log("NIM::IsPendingAutoTitleDownloadTasks\n");

	mem.write32(messagePointer, IPC::responseHeader(0x15, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, 0);  // Has pending tasks
}

void NIMService::getBackgroundEventForMenu(u32 messagePointer) {
	log("NIM::GetBackgroundEventForMenu\n");

	if (!backgroundSystemUpdateEvent.has_value()) {
		backgroundSystemUpdateEvent = kernel.makeEvent(ResetType::OneShot);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0x04000000);
	mem.write32(messagePointer + 12, backgroundSystemUpdateEvent.value());
}