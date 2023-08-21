#include "services/nfc.hpp"
#include "ipc.hpp"
#include "kernel.hpp"

namespace NFCCommands {
	enum : u32 {
		Initialize = 0x00010040,
		StartCommunication = 0x00030000,
		StopCommunication = 0x00040000,
		GetTagInRangeEvent = 0x000B0000,
		GetTagOutOfRangeEvent = 0x000C0000,
		GetTagState = 0x000D0000,
		CommunicationGetStatus = 0x000F0000,
		CommunicationGetResult = 0x00120000,
	};
}

void NFCService::reset() {
	tagInRangeEvent = std::nullopt;
	tagOutOfRangeEvent = std::nullopt;

	adapterStatus = Old3DSAdapterStatus::Idle;
	tagStatus = TagStatus::NotInitialized;
	initialized = false;
}

void NFCService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case NFCCommands::CommunicationGetStatus: communicationGetStatus(messagePointer); break;
		case NFCCommands::Initialize: initialize(messagePointer); break;
		case NFCCommands::GetTagInRangeEvent: getTagInRangeEvent(messagePointer); break;
		case NFCCommands::GetTagOutOfRangeEvent: getTagOutOfRangeEvent(messagePointer); break;
		case NFCCommands::GetTagState: getTagState(messagePointer); break;
		case NFCCommands::StartCommunication: startCommunication(messagePointer); break;
		case NFCCommands::StopCommunication: stopCommunication(messagePointer); break;
		default: Helpers::panic("NFC service requested. Command: %08X\n", command);
	}
}

void NFCService::initialize(u32 messagePointer) {
	const u8 type = mem.read8(messagePointer + 4);
	log("NFC::Initialize (type = %d)\n", type);

	adapterStatus = Old3DSAdapterStatus::InitializationComplete;
	tagStatus = TagStatus::Initialized;
	initialized = true;
	// TODO: This should error if already initialized. Also sanitize type.
	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

/*
	The NFC service provides userland with 2 events. One that is signaled when an NFC tag gets in range,
	And one that is signaled when it gets out of range. Userland can have a thread sleep on this so it will be alerted
	Whenever an Amiibo or misc NFC tag is presented or removed.
	These events are retrieved via the GetTagInRangeEvent and GetTagOutOfRangeEvent function respectively
*/

void NFCService::getTagInRangeEvent(u32 messagePointer) {
	log("NFC::GetTagInRangeEvent\n");

	// Create event if it doesn't exist
	if (!tagInRangeEvent.has_value()) {
		tagInRangeEvent = kernel.makeEvent(ResetType::OneShot);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x0B, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	// TODO: Translation descriptor here
	mem.write32(messagePointer + 12, tagInRangeEvent.value());
}

void NFCService::getTagOutOfRangeEvent(u32 messagePointer) {
	log("NFC::GetTagOutOfRangeEvent\n");

	// Create event if it doesn't exist
	if (!tagOutOfRangeEvent.has_value()) {
		tagOutOfRangeEvent = kernel.makeEvent(ResetType::OneShot);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x0C, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	// TODO: Translation descriptor here
	mem.write32(messagePointer + 12, tagOutOfRangeEvent.value());
}

void NFCService::getTagState(u32 messagePointer) {
	log("NFC::GetTagState");

	mem.write32(messagePointer, IPC::responseHeader(0xD, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, static_cast<u8>(tagStatus));
}

void NFCService::communicationGetStatus(u32 messagePointer) {
	log("NFC::CommunicationGetStatus");

	if (!initialized) {
		Helpers::warn("NFC::CommunicationGetStatus: Old 3DS NFC Adapter not initialized\n");
	}

	mem.write32(messagePointer, IPC::responseHeader(0xF, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, static_cast<u32>(adapterStatus));
}

void NFCService::startCommunication(u32 messagePointer) {
	log("NFC::StartCommunication\n");
	// adapterStatus = Old3DSAdapterStatus::Active;
	// TODO: Actually start communication when we emulate amiibo

	mem.write32(messagePointer, IPC::responseHeader(0x3, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::stopCommunication(u32 messagePointer) {
	log("NFC::StopCommunication\n");
	adapterStatus = Old3DSAdapterStatus::InitializationComplete;
	// TODO: Actually stop communication when we emulate amiibo

	mem.write32(messagePointer, IPC::responseHeader(0x4, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}