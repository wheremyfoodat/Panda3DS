#include "services/dsp.hpp"

namespace DSPCommands {
	enum : u32 {
		SetSemaphore = 0x00070040,
		ConvertProcessAddressFromDspDram = 0x000C0040,
		WriteProcessPipe = 0x000D0082,
		ReadPipeIfPossible = 0x001000C0,
		LoadComponent = 0x001100C2,
		FlushDataCache = 0x00130082,
		RegisterInterruptEvents = 0x00150082,
		GetSemaphoreHandle = 0x00160000,
		SetSemaphoreMask = 0x00170040,
		GetHeadphoneStatus = 0x001F0000
	};
}

namespace Result {
	enum : u32 {
		Success = 0,
		HeadphonesNotInserted = 0,
		HeadphonesInserted = 1
	};
}

void DSPService::reset() {
	audioPipe.reset();
}

void DSPService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case DSPCommands::ConvertProcessAddressFromDspDram: convertProcessAddressFromDspDram(messagePointer); break;
		case DSPCommands::FlushDataCache: flushDataCache(messagePointer); break;
		case DSPCommands::GetHeadphoneStatus: getHeadphoneStatus(messagePointer); break;
		case DSPCommands::GetSemaphoreHandle: getSemaphoreHandle(messagePointer); break;
		case DSPCommands::LoadComponent: loadComponent(messagePointer); break;
		case DSPCommands::ReadPipeIfPossible: readPipeIfPossible(messagePointer); break;
		case DSPCommands::RegisterInterruptEvents: registerInterruptEvents(messagePointer); break;
		case DSPCommands::SetSemaphore: setSemaphore(messagePointer); break;
		case DSPCommands::SetSemaphoreMask: setSemaphoreMask(messagePointer); break;
		case DSPCommands::WriteProcessPipe: [[likely]] writeProcessPipe(messagePointer); break;
		default: Helpers::panic("DSP service requested. Command: %08X\n", command);
	}
}

void DSPService::convertProcessAddressFromDspDram(u32 messagePointer) {
	const u32 address = mem.read32(messagePointer + 4);
	log("DSP::ConvertProcessAddressFromDspDram (address = %08X)\n", address);

	const u32 converted = (address << 1) + 0x1FF40000;
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, converted); // Converted address
}

void DSPService::loadComponent(u32 messagePointer) {
	u32 size = mem.read32(messagePointer + 4);
	u32 programMask = mem.read32(messagePointer + 8);
	u32 dataMask = mem.read32(messagePointer + 12);

	log("DSP::LoadComponent (size = %08X, program mask = %X, data mask = %X\n", size, programMask, dataMask);
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 1); // Component loaded
	mem.write32(messagePointer + 12, (size << 4) | 0xA);
	mem.write32(messagePointer + 16, mem.read32(messagePointer + 20)); // Component buffer
}

void DSPService::readPipeIfPossible(u32 messagePointer) {
	u32 channel = mem.read32(messagePointer + 4);
	u32 peer = mem.read32(messagePointer + 8);
	u16 size = mem.read16(messagePointer + 12);
	u32 buffer = mem.read32(messagePointer + 0x100 + 4);
	log("DSP::ReadPipeIfPossible (channel = %d, peer = %d, size = %04X, buffer = %08X)\n", channel, peer, size, buffer);

	if (size & 1) Helpers::panic("Tried to read odd amount of bytes from DSP pipe");
	if (channel != 2) Helpers::panic("Read from non-audio pipe");

	DSPPipe& pipe = audioPipe;

	uint i; // Number of bytes transferred
	for (i = 0; i < size; i += 2) {
		if (pipe.empty()) {
			printf("Tried to read from empty pipe\n");
			break;
		}

		mem.write16(buffer + i, pipe.readUnchecked());
	}

	mem.write32(messagePointer + 4, Result::Success);
	mem.write16(messagePointer + 8, i); // Number of bytes read
}

void DSPService::registerInterruptEvents(u32 messagePointer) {
	u32 interrupt = mem.read32(messagePointer + 4);
	u32 channel = mem.read32(messagePointer + 8);
	u32 event = mem.read32(messagePointer + 16);

	log("DSP::RegisterInterruptEvents (interrupt = %d, channel = %d, event = %d)\n", interrupt, channel, event);
	mem.write32(messagePointer + 4, Result::Success);
}

void DSPService::getHeadphoneStatus(u32 messagePointer) {
	log("DSP::GetHeadphoneStatus\n");

	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, Result::HeadphonesInserted); // This should be toggleable for shits and giggles
}

void DSPService::getSemaphoreHandle(u32 messagePointer) {
	log("DSP::GetSemaphoreHandle\n");

	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 12, 0xF9991234); // Semaphore handle (stubbed with random, obvious number)
}

void DSPService::setSemaphore(u32 messagePointer) {
	const u16 value = mem.read16(messagePointer + 4);
	log("DSP::SetSemaphore(value = %04X)\n", value);

	mem.write32(messagePointer + 4, Result::Success);
}

void DSPService::setSemaphoreMask(u32 messagePointer) {
	const u16 mask = mem.read16(messagePointer + 4);
	log("DSP::SetSemaphoreMask(mask = %04X)\n", mask);

	mem.write32(messagePointer + 4, Result::Success);
}

void DSPService::writeProcessPipe(u32 messagePointer) {
	u32 channel = mem.read32(messagePointer + 4);
	u32 size = mem.read32(messagePointer + 8);
	u32 buffer = mem.read32(messagePointer + 16);

	log("DSP::writeProcessPipe (channel = %d, size = %X, buffer = %08X)\n", channel, size, buffer);
	mem.write32(messagePointer + 4, Result::Success);
}

void DSPService::flushDataCache(u32 messagePointer) {
	u32 address = mem.read32(messagePointer + 4);
	u32 size = mem.read32(messagePointer + 8);
	u32 process = mem.read32(messagePointer + 16);

	log("DSP::FlushDataCache (addr = %08X, size = %08X, process = %X)\n", address, size, process);
	mem.write32(messagePointer + 4, Result::Success);
}