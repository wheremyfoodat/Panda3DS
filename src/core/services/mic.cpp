#include "services/mic.hpp"
#include "ipc.hpp"

namespace MICCommands {
	enum : u32 {
		MapSharedMem = 0x00010042,
		StartSampling = 0x00030140,
		SetGain = 0x00080040,
		GetGain = 0x00090000,
		SetPower = 0x000A0040,
		SetClamp = 0x000D0040,
		CaptainToadFunction = 0x00100040
	};
}

void MICService::reset() {
	micEnabled = false;
	shouldClamp = false;
	gain = 0;
}

void MICService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case MICCommands::GetGain: getGain(messagePointer); break;
		case MICCommands::MapSharedMem: mapSharedMem(messagePointer); break;
		case MICCommands::SetClamp: setClamp(messagePointer); break;
		case MICCommands::SetGain: setGain(messagePointer); break;
		case MICCommands::SetPower: setPower(messagePointer); break;
		case MICCommands::StartSampling: startSampling(messagePointer); break;
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

	log("MIC::StartSampling (encoding = %d, sample rate = %d, offset = %08X, size = %08X, loop: %s) (stubbed)\n",
		encoding, sampleRate, offset, dataSize, loop ? "yes" : "no"
	);

	mem.write32(messagePointer, IPC::responseHeader(0x3, 1, 0));
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