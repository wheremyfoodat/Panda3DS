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
		StopTagScanning = 0x00060000,
		GetTagInRangeEvent = 0x000B0000,
		GetTagOutOfRangeEvent = 0x000C0000,
		GetTagState = 0x000D0000,
		CommunicationGetStatus = 0x000F0000,
		GetTagInfo = 0x00110000,
		CommunicationGetResult = 0x00120000,
		LoadAmiiboPartially = 0x001A0000,
		GetModelInfo = 0x001B0000,
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
		case NFCCommands::GetModelInfo: getModelInfo(messagePointer); break;
		case NFCCommands::GetTagInfo: getTagInfo(messagePointer); break;
		case NFCCommands::GetTagInRangeEvent: getTagInRangeEvent(messagePointer); break;
		case NFCCommands::GetTagOutOfRangeEvent: getTagOutOfRangeEvent(messagePointer); break;
		case NFCCommands::GetTagState: getTagState(messagePointer); break;
		case NFCCommands::LoadAmiiboPartially: loadAmiiboPartially(messagePointer); break;
		case NFCCommands::Shutdown: shutdown(messagePointer); break;
		case NFCCommands::StartCommunication: startCommunication(messagePointer); break;
		case NFCCommands::StartTagScanning: startTagScanning(messagePointer); break;
		case NFCCommands::StopCommunication: stopCommunication(messagePointer); break;
		case NFCCommands::StopTagScanning: stopTagScanning(messagePointer); break;
		default: Helpers::panic("NFC service requested. Command: %08X\n", command);
	}
}

bool NFCService::loadAmiibo(const std::filesystem::path& path) {
	if (!initialized || tagStatus != TagStatus::Scanning) {
		Helpers::warn("It's not the correct time to load an amiibo! Make sure to load amiibi when the game is searching for one!");
		return false;
	}

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

	device.loadFromRaw();

	if (tagOutOfRangeEvent.has_value()) {
		kernel.clearEvent(tagOutOfRangeEvent.value());
	}

	if (tagInRangeEvent.has_value()) {
		kernel.signalEvent(tagInRangeEvent.value());
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
	if (!initialized) {
		Helpers::warn("Scanning for NFC tags before NFC service is initialized");
	}

	tagStatus = TagStatus::Scanning;

	mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::stopTagScanning(u32 messagePointer) {
	log("NFC::StopTagScanning\n");
	if (!initialized) {
		Helpers::warn("Stopping scanning for NFC tags before NFC service is initialized");
	}

	tagStatus = TagStatus::Initialized;

	mem.write32(messagePointer, IPC::responseHeader(0x6, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::stopCommunication(u32 messagePointer) {
	log("NFC::StopCommunication\n");
	adapterStatus = Old3DSAdapterStatus::InitializationComplete;
	// TODO: Actually stop communication when we emulate amiibo

	mem.write32(messagePointer, IPC::responseHeader(0x4, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::getTagInfo(u32 messagePointer) {
	log("NFC::GetTagInfo\n");
	Helpers::warn("Unimplemented NFC::GetTagInfo");

	mem.write32(messagePointer, IPC::responseHeader(0x11, 12, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::loadAmiiboPartially(u32 messagePointer) {
	log("NFC::LoadAmiiboPartially\n");
	Helpers::warn("Unimplemented NFC::LoadAmiiboPartially");

	mem.write32(messagePointer, IPC::responseHeader(0x1A, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void NFCService::getModelInfo(u32 messagePointer) {
	log("NFC::GetModelInfo\n");
	Helpers::warn("Unimplemented NFC::GetModelInfo");

	mem.write32(messagePointer, IPC::responseHeader(0x1B, 14, 0));
	mem.write32(messagePointer + 4, Result::Success);
}