#include "audio/teakra_core.hpp"

using namespace Audio;

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

TeakraDSP::TeakraDSP(Memory& mem) : DSPCore(mem), pipeBaseAddr(0), running(false) {
	teakra.Reset();

	// Set up callbacks for Teakra
	Teakra::AHBMCallback ahbm;

	ahbm.read8 = [&](u32 addr) -> u8 { return mem.read8(addr); };
	ahbm.read16 = [&](u32 addr) -> u16 { return mem.read16(addr); };
	ahbm.read32 = [&](u32 addr) -> u32 { return mem.read32(addr); };

	ahbm.write8 = [&](u32 addr, u8 value) { mem.write8(addr, value); };
	ahbm.write16 = [&](u32 addr, u16 value) { mem.write16(addr, value); };
	ahbm.write32 = [&](u32 addr, u32 value) { mem.write32(addr, value); };

	teakra.SetAHBMCallback(ahbm);
	teakra.SetAudioCallback([=](std::array<s16, 2> sample) {
		// NOP for now
	});
}

void TeakraDSP::reset() {
	teakra.Reset();
	running = false;
}

void TeakraDSP::writeProcessPipe(u32 channel, u32 size, u32 buffer) {
	// TODO
}

std::vector<u8> TeakraDSP::readPipe(u32 channel, u32 peer, u32 size, u32 buffer) {
	// TODO
	return std::vector<u8>();
}

void TeakraDSP::loadComponent(std::vector<u8>& data, u32 programMask, u32 dataMask) {
	// TODO: maybe move this to the DSP service

	u8* dspCode = teakra.GetDspMemory().data();
	u8* dspData = dspCode + 0x40000;

	Dsp1 dsp1;
	memcpy(&dsp1, data.data(), sizeof(dsp1));

	// TODO: verify DSP1 signature

	// Load DSP segments to DSP RAM
	// TODO: verify hashes
	for (unsigned int i = 0; i < dsp1.segmentCount; i++) {
		auto& segment = dsp1.segments[i];
		u32 addr = segment.dspAddr << 1;
		u8* src = data.data() + segment.offs;
		u8* dst = nullptr;

		switch (segment.type) {
			case 0:
			case 1: dst = dspCode + addr; break;
			default: dst = dspData + addr; break;
		}

		memcpy(dst, src, segment.size);
	}

	bool syncWithDsp = dsp1.flags & 0x1;
	bool loadSpecialSegment = (dsp1.flags >> 1) & 0x1;

	// TODO: how does the special segment work?
	if (loadSpecialSegment) {
		log("LoadComponent: special segment not supported");
	}

	running = true;

	if (syncWithDsp) {
		// Wait for the DSP to reply with 1s in all RECV registers
		for (int i = 0; i < 3; i++) {
			do {
				while (!teakra.RecvDataIsReady(i)) {
					runAudioFrame();
				}
			} while (teakra.RecvData(i) != 1);
		}
	}

	// Retrieve the pipe base address
	while (!teakra.RecvDataIsReady(2)) {
		runAudioFrame();
	}

	pipeBaseAddr = teakra.RecvData(2);
}

void TeakraDSP::unloadComponent() {
	if (!running) {
		Helpers::panic("Audio: unloadComponent called without a running program");
	}

	// Wait for SEND2 to be ready, then send the shutdown command to the DSP
	while (!teakra.SendDataIsEmpty(2)) {
		runAudioFrame();
	}

	teakra.SendData(2, 0x8000);

	// Wait for shutdown to be acknowledged
	while (!teakra.RecvDataIsReady(2)) {
		runAudioFrame();
	}

	// Read the value and discard it, completing shutdown
	teakra.RecvData(2);
	running = false;
}