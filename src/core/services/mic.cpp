#include "services/mic.hpp"

#include "ipc.hpp"
#include "kernel/kernel.hpp"

namespace MICCommands {
	enum : u32 {
		MapSharedMem = 0x00010042,
		UnmapSharedMem = 0x00020000,
		StartSampling = 0x00030140,
		StopSampling = 0x00050000,
		IsSampling = 0x00060000,
		GetEventHandle = 0x00070000,
		SetGain = 0x00080040,
		GetGain = 0x00090000,
		SetPower = 0x000A0040,
		GetPower = 0x000B0000,
		SetIirFilter = 0x000C0042,
		SetClamp = 0x000D0040,
		CaptainToadFunction = 0x00100040,
	};
}

void MICService::reset() {
	micEnabled = false;
	shouldClamp = false;
	currentlySampling = false;
	gain = 0;

	eventHandle = std::nullopt;
}

void MICService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case MICCommands::GetEventHandle: getEventHandle(messagePointer); break;
		case MICCommands::GetGain: getGain(messagePointer); break;
		case MICCommands::GetPower: getPower(messagePointer); break;
		case MICCommands::IsSampling: isSampling(messagePointer); break;
		case MICCommands::MapSharedMem: mapSharedMem(messagePointer); break;
		case MICCommands::SetClamp: setClamp(messagePointer); break;
		case MICCommands::SetGain: setGain(messagePointer); break;
		case MICCommands::SetIirFilter: setIirFilter(messagePointer); break;
		case MICCommands::SetPower: setPower(messagePointer); break;
		case MICCommands::StartSampling: startSampling(messagePointer); break;
		case MICCommands::StopSampling: stopSampling(messagePointer); break;
		case MICCommands::UnmapSharedMem: unmapSharedMem(messagePointer); break;
		case MICCommands::CaptainToadFunction: theCaptainToadFunction(messagePointer); break;
		default: Helpers::panic("MIC service requested. Command: %08X\n", command);
	}
}

void MICService::mapSharedMem(u32 messagePointer) {
	u32 size = mem.read32(messagePointer + 4);
	u32 handle = mem.read32(messagePointer + 12);

	log("MIC::MapSharedMem (size = %08X, handle = %X) (stubbed)\n", size, handle);
	mem.write32(messagePointer, IPC::responseHeader(0x1, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void MICService::unmapSharedMem(u32 messagePointer) {
	log("MIC::UnmapSharedMem (stubbed)\n");

	mem.write32(messagePointer, IPC::responseHeader(0x2, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void MICService::getEventHandle(u32 messagePointer) {
	log("MIC::GetEventHandle\n");
	Helpers::warn("Acquire MIC event handle");

	if (!eventHandle.has_value()) {
		eventHandle = kernel.makeEvent(ResetType::OneShot);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x7, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	// TODO: Translation descriptor
	mem.write32(messagePointer + 12, eventHandle.value());
}

void MICService::getGain(u32 messagePointer) {
	log("MIC::GetGain\n");
	mem.write32(messagePointer, IPC::responseHeader(0x9, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, gain);
}

void MICService::setGain(u32 messagePointer) {
	gain = mem.read8(messagePointer + 4);
	log("MIC::SetGain (value = %d)\n", gain);

	mem.write32(messagePointer, IPC::responseHeader(0x8, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void MICService::setPower(u32 messagePointer) {
	u8 val = mem.read8(messagePointer + 4);
	log("MIC::SetPower (value = %d)\n", val);

	micEnabled = val != 0;
	mem.write32(messagePointer, IPC::responseHeader(0xA, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void MICService::getPower(u32 messagePointer) {
	log("MIC::GetPower\n");

	mem.write32(messagePointer, IPC::responseHeader(0xB, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, micEnabled ? 1 : 0);
}

void MICService::setClamp(u32 messagePointer) {
	u8 val = mem.read8(messagePointer + 4);
	log("MIC::SetClamp (value = %d)\n", val);

	shouldClamp = val != 0;
	mem.write32(messagePointer, IPC::responseHeader(0xD, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void MICService::startSampling(u32 messagePointer) {
	u8 encoding = mem.read8(messagePointer + 4);
	u8 sampleRate = mem.read8(messagePointer + 8);
	u32 offset = mem.read32(messagePointer + 12);
	u32 dataSize = mem.read32(messagePointer + 16);
	bool loop = mem.read8(messagePointer + 20);

	log("MIC::StartSampling (encoding = %d, sample rate = %d, offset = %08X, size = %08X, loop: %s) (stubbed)\n", encoding, sampleRate, offset,
		dataSize, loop ? "yes" : "no");

	currentlySampling = true;
	mem.write32(messagePointer, IPC::responseHeader(0x3, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void MICService::stopSampling(u32 messagePointer) {
	log("MIC::StopSampling\n");
	currentlySampling = false;

	mem.write32(messagePointer, IPC::responseHeader(0x5, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void MICService::isSampling(u32 messagePointer) {
	log("MIC::IsSampling");

	mem.write32(messagePointer, IPC::responseHeader(0x6, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write8(messagePointer + 8, currentlySampling ? 1 : 0);
}

void MICService::setIirFilter(u32 messagePointer) {
	const u32 size = mem.read32(messagePointer + 4);
	const u32 pointer = mem.read32(messagePointer + 12);
	log("MIC::SetIirFilter (size = %X, pointer = %08X) (Stubbed)\n", size, pointer);

	mem.write32(messagePointer, IPC::responseHeader(0x0C, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
}

// Found in Captain Toad: Treasure Tracker
// This is what 3DBrew says:
// When the input value is 0, value 1 is written to an u8 MIC module state field.
// Otherwise, value 0 is written there.Normally the input value is non - zero.
// Citra calls it setClientVersion but no idea how they got that
void MICService::theCaptainToadFunction(u32 messagePointer) {
	log("MIC: Unknown function 0x00100040\n");

	mem.write32(messagePointer, IPC::responseHeader(0x10, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}