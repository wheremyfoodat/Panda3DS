#include "services/nfc.hpp"
#include "io_file.hpp"
#include "ipc.hpp"
#include "kernel.hpp"

namespace NFCCommands {
	enum : u32 {
		Initialize = 0x00010040,
		Shutdown = 0x00020040,
		StartCommunication = 0x00030000,
		StopCommunication = 0x00040000,
		StartTagScanning = 0x00050040,
		GetTagInRangeEvent = 0x000B0000,
		GetTagOutOfRangeEvent = 0x000C0000,
		GetTagState = 0x000D0000,
		CommunicationGetStatus = 0x000F0000,
		CommunicationGetResult = 0x00120000,
	};
}

void NFCService::reset() {
	device.reset();
	tagInRangeEvent = std::nullopt;
	tagOutOfRangeEvent = std::nullopt;

	adapterStatus = Old3DSAdapterStatus::Idle;
	tagStatus = TagStatus::NotInitialized;
	initialized = false;
}

void NFCService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case NFCCommands::CommunicationGetResult: communicationGetResult(messagePointer); break;
		case NFCCommands::CommunicationGetStatus: communicationGetStatus(messagePointer); break;
		case NFCCommands::Initialize: initialize(messagePointer); break;
		case NFCCommands::GetTagInRangeEvent: getTagInRangeEvent(messagePointer); break;
		case NFCCommands::GetTagOutOfRangeEvent: getTagOutOfRangeEvent(messagePointer); break;
		case NFCCommands::GetTagState: getTagState(messagePointer); break;
		case NFCCommands::Shutdown: shutdown(messagePointer); break;
		case NFCCommands::StartCommunication: startCommunication(messagePointer); break;
		case NFCCommands::StartTagScanning: startTagScanning(messagePointer); break;
		case NFCCommands::StopCommunication: stopCommunication(messagePointer); break;
		default: Helpers::panic("NFC service requested. Command: %08X\n", command);
	}
}

bool NFCService::loadAmiibo(const std::filesystem::path& path) {
	IOFile file(path, "rb");

	if (!file.isOpen()) {
		printf("Failed to open Amiibo file");
		file.close();

		return false;
	}

	auto [success, bytesRead] = file.readBytes(&device.raw, AmiiboDevice::tagSize);
	if (!success || bytesRead != AmiiboDevice::tagSize) {
		printf("Failed to read entire tag from Amiibo file: File might not be a proper amiibo file\n");
		file.close();

		return false;
	}

	file.close();
	return true;
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

void NFCService::shutdown(u32 messagePointer) {
	log("MFC::Shutdown");
	const u8 mode = mem.read8(messagePointer + 4);

	Helpers::warn("NFC::Shutdown: Unimplemented mode: %d", mode);

	mem.write32(messagePointer, IPC::responseHeader(0x2, 1, 0));
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
	log("NFC::GetTagState\n");

	mem.write32(messagePointer, IPC::responseHeader(0xD, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, static_cast<u8>(tagStatus));
}

void NFCService::communicationGetStatus(u32 messagePointer) {
	log("NFC::CommunicationGetStatus\n");

	if (!initialized) {
		Helpers::warn("NFC::CommunicationGetStatus: Old 3DS NFC Adapter not initialized\n");
	}

	mem.write32(messagePointer, IPC::responseHeader(0xF, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, static_cast<u32>(adapterStatus));
}


void NFCService::communicationGetResult(u32 messagePointer) {
	log("NFC::CommunicationGetResult\n");

	if (!initialized) {
		Helpers::warn("NFC::CommunicationGetResult: Old 3DS NFC Adapter not initialized\n");
	}

	mem.write32(messagePointer, IPC::responseHeader(0x12, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	// On N3DS: This always writes 0 here
	// On O3DS with the NFC adapter: Returns a result code for NFC communication
	mem.write32(messagePointer + 8, 0);
}

void NFCService::startCommunication(u32 messagePointer) {
	log("NFC::StartCommunication\n");
	// adapterStatus = Old3DSAdapterStatus::Active;
	// TODO: Actually start communication when we emulate amiibo

	mem.write32(messagePointer, IPC::responseHeader(0x3, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::startTagScanning(u32 messagePointer) {
	log("NFC::StartTagScanning\n");
	tagStatus = TagStatus::Scanning;

	mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::stopCommunication(u32 messagePointer) {
	log("NFC::StopCommunication\n");
	adapterStatus = Old3DSAdapterStatus::InitializationComplete;
	// TODO: Actually stop communication when we emulate amiibo

	mem.write32(messagePointer, IPC::responseHeader(0x4, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}