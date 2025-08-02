#include "services/ssl.hpp"

#include "ipc.hpp"
#include "result/result.hpp"

namespace SSLCommands {
	enum : u32 {
		Initialize = 0x00010002,
		GenerateRandomData = 0x00110042,
	};
}

void SSLService::reset() {
	initialized = false;

	// Use the default seed on reset to avoid funny bugs
	rng.seed();
}

void SSLService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case SSLCommands::Initialize: initialize(messagePointer); break;
		case SSLCommands::GenerateRandomData: generateRandomData(messagePointer); break;
		default: Helpers::panic("SSL service requested. Command: %08X\n", command);
	}
}

void SSLService::initialize(u32 messagePointer) {
	log("SSL::Initialize\n");
	mem.write32(messagePointer, IPC::responseHeader(0x01, 1, 0));

	if (initialized) {
		Helpers::warn("SSL service initialized twice");
	}

	initialized = true;
	rng.seed(std::random_device()());  // Seed rng via std::random_device

	mem.write32(messagePointer + 4, Result::Success);
}

void SSLService::generateRandomData(u32 messagePointer) {
	const u32 size = mem.read32(messagePointer + 4);
	const u32 output = mem.read32(messagePointer + 12);
	log("SSL::GenerateRandomData (out = %08X, size = %08X)\n", output, size);

	// TODO: This might be a biiit slow, might want to make it write in word quantities
	u32 data;

	for (u32 i = 0; i < size; i++) {
		// We don't have an available random value since we're on a multiple of 4 bytes and our Twister is 32-bit, generate a new one from the
		// Mersenne Twister
		if ((i & 3) == 0) {
			data = rng();
		}

		mem.write8(output + i, u8(data));
		// Shift data by 8 to get the next byte
		data >>= 8;
	}

	mem.write32(messagePointer, IPC::responseHeader(0x11, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
}