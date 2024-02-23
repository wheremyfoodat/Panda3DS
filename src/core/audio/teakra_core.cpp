#include "audio/teakra_core.hpp"

#include <algorithm>
#include <cstring>

#include "services/dsp.hpp"

using namespace Audio;
static constexpr u32 sampleRate = 32768;
static constexpr u32 duration = 30;
static s16 samples[sampleRate * duration * 2];
static uint sampleIndex = 0;

struct Dsp1 {
	// All sizes are in bytes unless otherwise specified
	u8 signature[0x100];
	u8 magic[4];
	u32 size;
	u8 codeMemLayout;
	u8 dataMemLayout;
	u8 pad[3];
	u8 specialType;
	u8 segmentCount;
	u8 flags;
	u32 specialStart;
	u32 specialSize;
	u64 zeroBits;

	struct Segment {
		u32 offs;     // Offset of the segment data
		u32 dspAddr;  // Start of the segment in 16-bit units
		u32 size;
		u8 pad[3];
		u8 type;
		u8 hash[0x20];
	};

	Segment segments[10];
};

TeakraDSP::TeakraDSP(Memory& mem, Scheduler& scheduler, DSPService& dspService)
	: DSPCore(mem, scheduler, dspService), pipeBaseAddr(0), running(false) {
	// Set up callbacks for Teakra
	Teakra::AHBMCallback ahbm;

	// The AHBM read handlers read from paddrs rather than vaddrs which mem.read8 and the like use
	// TODO: When we implement more efficient paddr accesses with a page table or similar, these handlers
	// Should be made to properly use it, since this method is hacky and will segfault if given an invalid addr
	ahbm.read8 = [&](u32 addr) -> u8 { return mem.getFCRAM()[addr - PhysicalAddrs::FCRAM]; };
	ahbm.read16 = [&](u32 addr) -> u16 { return *(u16*)&mem.getFCRAM()[addr - PhysicalAddrs::FCRAM]; };
	ahbm.read32 = [&](u32 addr) -> u32 { return *(u32*)&mem.getFCRAM()[addr - PhysicalAddrs::FCRAM]; };

	ahbm.write8 = [&](u32 addr, u8 value) { mem.getFCRAM()[addr - PhysicalAddrs::FCRAM] = value; };
	ahbm.write16 = [&](u32 addr, u16 value) { *(u16*)&mem.getFCRAM()[addr - PhysicalAddrs::FCRAM] = value; };
	ahbm.write32 = [&](u32 addr, u32 value) { *(u32*)&mem.getFCRAM()[addr - PhysicalAddrs::FCRAM] = value; };

	teakra.SetAHBMCallback(ahbm);
	teakra.SetAudioCallback([=](std::array<s16, 2> sample) { /* Do nothing */ });

	// Set up event handlers. These handlers forward a hardware interrupt to the DSP service, which is responsible
	// For triggering the appropriate DSP kernel events
	// Note: It's important not to fire any events if "loaded" is false, ie if we haven't fully loaded a DSP component yet
	teakra.SetRecvDataHandler(0, [&]() {
		if (loaded) {
			dspService.triggerInterrupt0();
		}
	});

	teakra.SetRecvDataHandler(1, [&]() {
		if (loaded) {
			dspService.triggerInterrupt1();
		}
	});

	auto processPipeEvent = [&](bool dataEvent) {
		if (!loaded) {
			return;
		}

		if (dataEvent) {
			signalledData = true;
		} else {
			if ((teakra.GetSemaphore() & 0x8000) == 0) {
				return;
			}

			signalledSemaphore = true;
		}

		if (signalledSemaphore && signalledData) {
			signalledSemaphore = signalledData = false;

			u16 slot = teakra.RecvData(2);
			u16 side = slot & 1;
			u16 pipe = slot / 2;

			if (side != static_cast<u16>(PipeDirection::DSPtoCPU)) {
				return;
			}

			if (pipe == 0) {
				Helpers::warn("Pipe event for debug pipe: Should be ignored and the data should be flushed");
			} else {
				dspService.triggerPipeEvent(pipe);
			}
		}
	};

	teakra.SetRecvDataHandler(2, [processPipeEvent]() { processPipeEvent(true); });
	teakra.SetSemaphoreHandler([processPipeEvent]() { processPipeEvent(false); });
}

void TeakraDSP::reset() {
	teakra.Reset();
	running = false;
	loaded = false;
	signalledData = signalledSemaphore = false;
}

void TeakraDSP::setAudioEnabled(bool enable) {
	if (audioEnabled != enable) {
		audioEnabled = enable;

		// Set the appropriate audio callback for Teakra
		if (audioEnabled) {
			teakra.SetAudioCallback([=](std::array<s16, 2> sample) {
				// Wait until we can push our samples
				while (sampleBuffer.size() + 2 > sampleBuffer.Capacity()) {
					printf("shit\n");
				}
				sampleBuffer.push(sample.data(), 2);
			});
		} else {
			teakra.SetAudioCallback([=](std::array<s16, 2> sample) { /* Do nothing */ });
		}
	}
}

// https://github.com/citra-emu/citra/blob/master/src/audio_core/lle/lle.cpp
void TeakraDSP::writeProcessPipe(u32 channel, u32 size, u32 buffer) {
	size &= 0xffff;

	PipeStatus status = getPipeStatus(channel, PipeDirection::CPUtoDSP);
	bool needUpdate = false;  // Do we need to update the pipe status and catch up Teakra?

	std::vector<u8> data;
	data.reserve(size);

	// Read data to write
	for (int i = 0; i < size; i++) {
		const u8 byte = mem.read8(buffer + i);
		data.push_back(byte);
	}
	u8* dataPointer = data.data();

	while (size != 0) {
		if (status.isFull()) {
			Helpers::warn("Teakra: Writing to full pipe");
		}

		// Calculate begin/end/size for write
		const u16 writeEnd = status.isWrapped() ? (status.readPointer & PipeStatus::pointerMask) : status.byteSize;
		const u16 writeBegin = status.writePointer & PipeStatus::pointerMask;
		const u16 writeSize = std::min<u16>(u16(size), writeEnd - writeBegin);

		if (writeEnd <= writeBegin) [[unlikely]] {
			Helpers::warn("Teakra: Writing to pipe but end <= start");
		}

		// Write data to pipe, increment write and buffer pointers, decrement size
		std::memcpy(getDataPointer(status.address * 2 + writeBegin), dataPointer, writeSize);
		dataPointer += writeSize;
		status.writePointer += writeSize;
		size -= writeSize;

		if ((status.writePointer & PipeStatus::pointerMask) > status.byteSize) [[unlikely]] {
			Helpers::warn("Teakra: Writing to pipe but write > size");
		}

		if ((status.writePointer & PipeStatus::pointerMask) == status.byteSize) {
			status.writePointer &= PipeStatus::wrapBit;
			status.writePointer ^= PipeStatus::wrapBit;
		}
		needUpdate = true;
	}

	if (needUpdate) {
		updatePipeStatus(status);
		while (!teakra.SendDataIsEmpty(2)) {
			runSlice();
		}

		teakra.SendData(2, status.slot);
	}
}

std::vector<u8> TeakraDSP::readPipe(u32 channel, u32 peer, u32 size, u32 buffer) {
	size &= 0xffff;

	PipeStatus status = getPipeStatus(channel, PipeDirection::DSPtoCPU);

	std::vector<u8> pipeData(size);
	u8* dataPointer = pipeData.data();
	bool needUpdate = false;  // Do we need to update the pipe status and catch up Teakra?

	while (size != 0) {
		if (status.isEmpty()) [[unlikely]] {
			Helpers::warn("Teakra: Reading from empty pipe");
			return pipeData;
		}

		// Read as many bytes as possible
		const u16 readEnd = status.isWrapped() ? status.byteSize : (status.writePointer & PipeStatus::pointerMask);
		const u16 readBegin = status.readPointer & PipeStatus::pointerMask;
		const u16 readSize = std::min<u16>(u16(size), readEnd - readBegin);

		// Copy bytes to the output vector, increment the read and vector pointers and decrement the size appropriately
		std::memcpy(dataPointer, getDataPointer(status.address * 2 + readBegin), readSize);
		dataPointer += readSize;
		status.readPointer += readSize;
		size -= readSize;

		if ((status.readPointer & PipeStatus::pointerMask) > status.byteSize) [[unlikely]] {
			Helpers::warn("Teakra: Reading from pipe but read > size");
		}

		if ((status.readPointer & PipeStatus::pointerMask) == status.byteSize) {
			status.readPointer &= PipeStatus::wrapBit;
			status.readPointer ^= PipeStatus::wrapBit;
		}

		needUpdate = true;
	}

	if (needUpdate) {
		updatePipeStatus(status);
		while (!teakra.SendDataIsEmpty(2)) {
			runSlice();
		}

		teakra.SendData(2, status.slot);
	}

	return pipeData;
}

void TeakraDSP::loadComponent(std::vector<u8>& data, u32 programMask, u32 dataMask) {
	// TODO: maybe move this to the DSP service
	if (loaded) {
		Helpers::warn("Loading DSP component when already loaded");
		return;
	}

	teakra.Reset();
	running = true;

	u8* dspCode = teakra.GetDspMemory().data();
	u8* dspData = dspCode + 0x40000;

	Dsp1 dsp1;
	std::memcpy(&dsp1, data.data(), sizeof(dsp1));

	// TODO: verify DSP1 signature

	// Load DSP segments to DSP RAM
	// TODO: verify hashes
	for (uint i = 0; i < dsp1.segmentCount; i++) {
		auto& segment = dsp1.segments[i];
		u32 addr = segment.dspAddr << 1;
		u8* src = data.data() + segment.offs;
		u8* dst = nullptr;

		switch (segment.type) {
			case 0:
			case 1: dst = dspCode + addr; break;
			default: dst = dspData + addr; break;
		}

		std::memcpy(dst, src, segment.size);
	}

	bool syncWithDsp = dsp1.flags & 0x1;
	bool loadSpecialSegment = (dsp1.flags >> 1) & 0x1;

	// TODO: how does the special segment work?
	if (loadSpecialSegment) {
		log("LoadComponent: special segment not supported");
	}

	if (syncWithDsp) {
		// Wait for the DSP to reply with 1s in all RECV registers
		for (int i = 0; i < 3; i++) {
			do {
				while (!teakra.RecvDataIsReady(i)) {
					runSlice();
				}
			} while (teakra.RecvData(i) != 1);
		}
	}

	// Retrieve the pipe base address
	while (!teakra.RecvDataIsReady(2)) {
		runSlice();
	}
	pipeBaseAddr = teakra.RecvData(2);
	
	// Schedule next DSP event
	scheduler.addEvent(Scheduler::EventType::RunDSP, scheduler.currentTimestamp + Audio::lleSlice * 2);
	loaded = true;
}

void TeakraDSP::unloadComponent() {
	if (!loaded) {
		Helpers::warn("Audio: unloadComponent called without a running program");
		return;
	}
	loaded = false;
	// Stop scheduling DSP events
	scheduler.removeEvent(Scheduler::EventType::RunDSP);

	// Wait for SEND2 to be ready, then send the shutdown command to the DSP
	while (!teakra.SendDataIsEmpty(2)) {
		runSlice();
	}

	teakra.SendData(2, 0x8000);

	// Wait for shutdown to be acknowledged
	while (!teakra.RecvDataIsReady(2)) {
		runSlice();
	}

	// Read the value and discard it, completing shutdown
	teakra.RecvData(2);
	running = false;
}