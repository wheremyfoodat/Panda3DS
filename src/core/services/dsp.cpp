#include "services/dsp.hpp"
#include "ipc.hpp"
#include "kernel.hpp"

#include <algorithm>

namespace DSPCommands {
	enum : u32 {
		RecvData = 0x00010040,
		RecvDataIsReady = 0x00020040,
		SetSemaphore = 0x00070040,
		ConvertProcessAddressFromDspDram = 0x000C0040,
		WriteProcessPipe = 0x000D0082,
		ReadPipeIfPossible = 0x001000C0,
		LoadComponent = 0x001100C2,
		UnloadComponent = 0x00120000,
		FlushDataCache = 0x00130082,
		InvalidateDataCache = 0x00140082,
		RegisterInterruptEvents = 0x00150082,
		GetSemaphoreEventHandle = 0x00160000,
		SetSemaphoreMask = 0x00170040,
		GetHeadphoneStatus = 0x001F0000
	};
}

namespace Result {
	enum : u32 {
		HeadphonesNotInserted = 0,
		HeadphonesInserted = 1
	};
}

void DSPService::reset() {
	for (auto& e : pipeData)
		e.clear();

	// Note: Reset audio pipe AFTER resetting all pipes, otherwise the new data will be yeeted
	resetAudioPipe();
	totalEventCount = 0;
	dspState = DSPState::Off;

	semaphoreEvent = std::nullopt;
	interrupt0 = std::nullopt;
	interrupt1 = std::nullopt;

	for (DSPEvent& e : pipeEvents) {
		e = std::nullopt;
	}
}

void DSPService::resetAudioPipe() {
	// Hardcoded responses for now
	// These are DSP DRAM offsets for various variables
	// https://www.3dbrew.org/wiki/DSP_Memory_Region
	static constexpr std::array<u16, 16> responses = {
		0x000F, // Number of responses
		0xBFFF, // Frame counter
		0x9E92, // Source configs
		0x8680, // Source statuses
		0xA792, // ADPCM coefficients
		0x9430, // DSP configs
		0x8400, // DSP status
		0x8540, // Final samples
		0x9492, // Intermediate mix samples
		0x8710, // Compressor
		0x8410, // Debug
		0xA912, // ??
		0xAA12, // ??
		0xAAD2, // ??
		0xAC52, // Surround sound biquad filter 1
		0xAC5C  // Surround sound biquad filter 2
	};

	std::vector<u8>& audioPipe = pipeData[DSPPipeType::Audio];
	audioPipe.resize(responses.size() * sizeof(u16));

	// Push back every response to the audio pipe
	size_t index = 0;
	for (auto e : responses) {
		audioPipe[index++] = e & 0xff;
		audioPipe[index++] = e >> 8;
	}
}

void DSPService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case DSPCommands::ConvertProcessAddressFromDspDram: convertProcessAddressFromDspDram(messagePointer); break;
		case DSPCommands::FlushDataCache: flushDataCache(messagePointer); break;
		case DSPCommands::InvalidateDataCache: invalidateDCache(messagePointer); break;
		case DSPCommands::GetHeadphoneStatus: getHeadphoneStatus(messagePointer); break;
		case DSPCommands::GetSemaphoreEventHandle: getSemaphoreEventHandle(messagePointer); break;
		case DSPCommands::LoadComponent: loadComponent(messagePointer); break;
		case DSPCommands::ReadPipeIfPossible: readPipeIfPossible(messagePointer); break;
		case DSPCommands::RecvData: [[likely]] recvData(messagePointer); break;
		case DSPCommands::RecvDataIsReady: [[likely]] recvDataIsReady(messagePointer); break;
		case DSPCommands::RegisterInterruptEvents: registerInterruptEvents(messagePointer); break;
		case DSPCommands::SetSemaphore: setSemaphore(messagePointer); break;
		case DSPCommands::SetSemaphoreMask: setSemaphoreMask(messagePointer); break;
		case DSPCommands::UnloadComponent: unloadComponent(messagePointer); break;
		case DSPCommands::WriteProcessPipe: [[likely]] writeProcessPipe(messagePointer); break;
		default: Helpers::panic("DSP service requested. Command: %08X\n", command);
	}
}

void DSPService::convertProcessAddressFromDspDram(u32 messagePointer) {
	const u32 address = mem.read32(messagePointer + 4);
	log("DSP::ConvertProcessAddressFromDspDram (address = %08X)\n", address);
	const u32 converted = (address << 1) + 0x1FF40000;

	mem.write32(messagePointer, IPC::responseHeader(0xC, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, converted); // Converted address
}

void DSPService::loadComponent(u32 messagePointer) {
	u32 size = mem.read32(messagePointer + 4);
	u32 programMask = mem.read32(messagePointer + 8);
	u32 dataMask = mem.read32(messagePointer + 12);

	log("DSP::LoadComponent (size = %08X, program mask = %X, data mask = %X\n", size, programMask, dataMask);
	mem.write32(messagePointer, IPC::responseHeader(0x11, 2, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 1); // Component loaded
	mem.write32(messagePointer + 12, (size << 4) | 0xA);
	mem.write32(messagePointer + 16, mem.read32(messagePointer + 20)); // Component buffer
}

void DSPService::unloadComponent(u32 messagePointer) {
	log("DSP::UnloadComponent\n");
	mem.write32(messagePointer, IPC::responseHeader(0x12, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

std::vector<u8> DSPService::readPipe(u32 pipe, u32 size) {
	if (size & 1) Helpers::panic("Tried to read odd amount of bytes from DSP pipe");
	if (pipe >= pipeCount || size > 0xffff) {
		return {};
	}

	if (pipe != DSPPipeType::Audio) {
		log("Reading from non-audio pipe! This might be broken, might need to check what pipe is being read from and implement writing to it\n");
	}

	std::vector<u8>& data = pipeData[pipe];
	size = std::min<u32>(size, data.size()); // Clamp size to the maximum available data size

	if (size == 0)
		return {};

	// Return "size" bytes from the audio pipe and erase them
	std::vector<u8> out(data.begin(), data.begin() + size);
	data.erase(data.begin(), data.begin() + size);
	return out;
}

void DSPService::readPipeIfPossible(u32 messagePointer) {
	u32 channel = mem.read32(messagePointer + 4);
	u32 peer = mem.read32(messagePointer + 8);
	u16 size = mem.read16(messagePointer + 12);
	u32 buffer = mem.read32(messagePointer + 0x100 + 4);
	log("DSP::ReadPipeIfPossible (channel = %d, peer = %d, size = %04X, buffer = %08X)\n", channel, peer, size, buffer);
	mem.write32(messagePointer, IPC::responseHeader(0x10, 2, 2));

	std::vector<u8> data = readPipe(channel, size);
	for (uint i = 0; i < data.size(); i++) {
		mem.write8(buffer + i, data[i]);
	}

	mem.write32(messagePointer + 4, Result::Success);
	mem.write16(messagePointer + 8, data.size()); // Number of bytes read
}

void DSPService::recvData(u32 messagePointer) {
	const u32 registerIndex = mem.read32(messagePointer + 4);
	log("DSP::RecvData (register = %d)\n", registerIndex);
	if (registerIndex != 0) Helpers::panic("Unknown register in DSP::RecvData");

	// Return 0 if the DSP is running, otherwise 1
	const u16 ret = dspState == DSPState::On ? 0 : 1;

	mem.write32(messagePointer, IPC::responseHeader(0x01, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write16(messagePointer + 8, ret);
}

void DSPService::recvDataIsReady(u32 messagePointer) {
	const u32 registerIndex = mem.read32(messagePointer + 4);
	log("DSP::RecvDataIsReady (register = %d)\n", registerIndex);
	if (registerIndex != 0) Helpers::panic("Unknown register in DSP::RecvDataIsReady");

	mem.write32(messagePointer, IPC::responseHeader(0x02, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 1); // Always return that the register is ready for now
}

DSPService::DSPEvent& DSPService::getEventRef(u32 type, u32 pipe) {
	switch (type) {
		case 0: return interrupt0;
		case 1: return interrupt1;

		case 2:
			if (pipe >= pipeCount)
				Helpers::panic("Tried to access the event of an invalid pipe");
			return pipeEvents[pipe];

		default:
			Helpers::panic("Unknown type for DSP::getEventRef");
	}
}

void DSPService::registerInterruptEvents(u32 messagePointer) {
	const u32 interrupt = mem.read32(messagePointer + 4);
	const u32 channel = mem.read32(messagePointer + 8);
	const u32 eventHandle = mem.read32(messagePointer + 16);
	log("DSP::RegisterInterruptEvents (interrupt = %d, channel = %d, event = %d)\n", interrupt, channel, eventHandle);

	// The event handle being 0 means we're removing an event
	if (eventHandle == 0) {
		DSPEvent& e = getEventRef(interrupt, channel); // Get event
		if (e.has_value()) { // Remove if it exists
			totalEventCount--;
			e = std::nullopt;
		}
	} else {
		const KernelObject* object = kernel.getObject(eventHandle, KernelObjectType::Event);
		if (!object) {
			Helpers::panic("DSP::DSP::RegisterInterruptEvents with invalid event handle");
		}

		if (totalEventCount >= maxEventCount)
			Helpers::panic("DSP::RegisterInterruptEvents overflowed total number of allowed events");
		else {
			getEventRef(interrupt, channel) = eventHandle;
			mem.write32(messagePointer, IPC::responseHeader(0x15, 1, 0));
			mem.write32(messagePointer + 4, Result::Success);

			totalEventCount++;
			kernel.signalEvent(eventHandle);
		}
	}
}

void DSPService::getHeadphoneStatus(u32 messagePointer) {
	log("DSP::GetHeadphoneStatus\n");

	mem.write32(messagePointer, IPC::responseHeader(0x1F, 2, 0));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, Result::HeadphonesInserted); // This should be toggleable for shits and giggles
}

void DSPService::getSemaphoreEventHandle(u32 messagePointer) {
	log("DSP::GetSemaphoreEventHandle\n");

	if (!semaphoreEvent.has_value()) {
		semaphoreEvent = kernel.makeEvent(ResetType::OneShot);
	}

	mem.write32(messagePointer, IPC::responseHeader(0x16, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	// TODO: Translation descriptor here?
	mem.write32(messagePointer + 12, semaphoreEvent.value()); // Semaphore event handle
	kernel.signalEvent(semaphoreEvent.value());
}

void DSPService::setSemaphore(u32 messagePointer) {
	const u16 value = mem.read16(messagePointer + 4);
	log("DSP::SetSemaphore(value = %04X)\n", value);

	mem.write32(messagePointer, IPC::responseHeader(0x7, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void DSPService::setSemaphoreMask(u32 messagePointer) {
	const u16 mask = mem.read16(messagePointer + 4);
	log("DSP::SetSemaphoreMask(mask = %04X)\n", mask);

	mem.write32(messagePointer, IPC::responseHeader(0x17, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void DSPService::writeProcessPipe(u32 messagePointer) {
	const u32 channel = mem.read32(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	const u32 buffer = mem.read32(messagePointer + 16);
	log("DSP::writeProcessPipe (channel = %d, size = %X, buffer = %08X)\n", channel, size, buffer);

	enum class StateChange : u8 {
		Initialize = 0,
		Shutdown = 1,
		Wakeup = 2,
		Sleep = 3,
	};

	switch (channel) {
		case DSPPipeType::Audio: {
			if (size != 4) {
				printf("Invalid size written to DSP Audio Pipe\n");
				break;
			}

			// Get new state
			const u8 state = mem.read8(buffer);
			if (state > 3) {
				log("WriteProcessPipe::Audio: Unknown state change type");
			} else {
				switch (static_cast<StateChange>(state)) {
					case StateChange::Initialize:
						// TODO: Other initialization stuff here
						dspState = DSPState::On;
						resetAudioPipe();
						break;

					case StateChange::Shutdown:
						dspState = DSPState::Off;
						break;

					default: Helpers::panic("Unimplemented DSP audio pipe state change %d", state);
				}
			}
			break;
		}

		case DSPPipeType::Binary:
			Helpers::warn("Unimplemented write to binary pipe! Size: %d\n", size);
			break;

		default:
			log("DSP: Wrote to unimplemented pipe %d\n", channel);
			break;
	}

	mem.write32(messagePointer, IPC::responseHeader(0xD, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void DSPService::flushDataCache(u32 messagePointer) {
	const u32 address = mem.read32(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	const Handle process = mem.read32(messagePointer + 16);

	log("DSP::FlushDataCache (addr = %08X, size = %08X, process = %X)\n", address, size, process);
	mem.write32(messagePointer, IPC::responseHeader(0x13, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void DSPService::invalidateDCache(u32 messagePointer) {
	const u32 address = mem.read32(messagePointer + 4);
	const u32 size = mem.read32(messagePointer + 8);
	const Handle process = mem.read32(messagePointer + 16);

	log("DSP::InvalidateDataCache (addr = %08X, size = %08X, process = %X)\n", address, size, process);
	mem.write32(messagePointer, IPC::responseHeader(0x14, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void DSPService::signalEvents() {
	for (const DSPEvent& e : pipeEvents) {
		if (e.has_value()) { kernel.signalEvent(e.value()); }
	}

	if (semaphoreEvent.has_value()) { kernel.signalEvent(semaphoreEvent.value()); }
	if (interrupt0.has_value()) { kernel.signalEvent(interrupt0.value()); }
	if (interrupt1.has_value()) { kernel.signalEvent(interrupt1.value()); }
}