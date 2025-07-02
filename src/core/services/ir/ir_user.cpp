#include "services/ir/ir_user.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "ipc.hpp"
#include "kernel.hpp"
#include "services/ir/ir_types.hpp"

using namespace IR;

namespace IRUserCommands {
	enum : u32 {
		FinalizeIrnop = 0x00020000,
		ClearReceiveBuffer = 0x00030000,
		ClearSendBuffer = 0x00040000,
		RequireConnection = 0x00060040,
		Disconnect = 0x00090000,
		GetReceiveEvent = 0x000A0000,
		GetSendEvent = 0x000B0000,
		GetConnectionStatusEvent = 0x000C0000,
		SendIrnop = 0x000D0042,
		InitializeIrnopShared = 0x00180182,
		ReleaseReceivedData = 0x00190040,
	};
}

IRUserService::IRUserService(Memory& mem, HIDService& hid, const EmulatorConfig& config, Kernel& kernel)
	: mem(mem), hid(hid), config(config), kernel(kernel), cpp([&](IR::Device::Payload payload) { sendPayload(payload); }, kernel.getScheduler()) {}

void IRUserService::reset() {
	connectionStatusEvent = std::nullopt;
	receiveEvent = std::nullopt;
	sendEvent = std::nullopt;
	sharedMemory = std::nullopt;

	receiveBuffer = nullptr;
	connectedDevice = false;
}

void IRUserService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case IRUserCommands::Disconnect: disconnect(messagePointer); break;
		case IRUserCommands::FinalizeIrnop: finalizeIrnop(messagePointer); break;
		case IRUserCommands::GetReceiveEvent: getReceiveEvent(messagePointer); break;
		case IRUserCommands::GetSendEvent: getSendEvent(messagePointer); break;
		case IRUserCommands::GetConnectionStatusEvent: getConnectionStatusEvent(messagePointer); break;
		case IRUserCommands::InitializeIrnopShared: initializeIrnopShared(messagePointer); break;
		case IRUserCommands::RequireConnection: requireConnection(messagePointer); break;
		case IRUserCommands::SendIrnop: sendIrnop(messagePointer); break;
		case IRUserCommands::ClearReceiveBuffer: clearReceiveBuffer(messagePointer); break;
		case IRUserCommands::ClearSendBuffer: clearSendBuffer(messagePointer); break;
		case IRUserCommands::ReleaseReceivedData: releaseReceivedData(messagePointer); break;
		default: Helpers::panic("ir:USER service requested. Command: %08X\n", command);
	}
}

void IRUserService::initializeIrnopShared(u32 messagePointer) {
	const u32 sharedMemSize = mem.read32(messagePointer + 4);
	const u32 receiveBufferSize = mem.read32(messagePointer + 8);
	const u32 receiveBufferPackageCount = mem.read32(messagePointer + 12);
	const u32 sendBufferSize = mem.read32(messagePointer + 16);
	const u32 sendBufferPackageCount = mem.read32(messagePointer + 20);
	const u32 bitrate = mem.read32(messagePointer + 24);
	const u32 descriptor = mem.read32(messagePointer + 28);
	const u32 sharedMemHandle = mem.read32(messagePointer + 32);

	log("IR:USER: InitializeIrnopShared (shared mem size = %08X, sharedMemHandle = %X)\n", sharedMemSize, sharedMemHandle);
	Helpers::warn("Game is initializing IR:USER. If it explodes, this is probably why");

	KernelObject* object = kernel.getObject(sharedMemHandle, KernelObjectType::MemoryBlock);
	if (object == nullptr) {
		Helpers::panic("IR::InitializeIrnopShared: Shared memory object does not exist");
	}

	MemoryBlock* memoryBlock = object->getData<MemoryBlock>();
	sharedMemory = *memoryBlock;

	receiveBuffer = std::make_unique<IR::Buffer>(mem, memoryBlock->addr, 0x10, 0x20, receiveBufferPackageCount, receiveBufferSize);

	// Initialize the shared memory block to 0s
	for (int i = 0; i < sizeof(SharedMemoryStatus); i++) {
		mem.write8(memoryBlock->addr + i, 0);
	}
	// Set the initialized byte in shared mem to 1
	mem.write8(memoryBlock->addr + offsetof(SharedMemoryStatus, isInitialized), 1);

	mem.write32(messagePointer, IPC::responseHeader(0x18, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void IRUserService::finalizeIrnop(u32 messagePointer) {
	log("IR:USER: FinalizeIrnop\n");

	if (connectedDevice) {
		connectedDevice = false;
		cpp.disconnect();
	}

	sharedMemory = std::nullopt;
	receiveBuffer = nullptr;

	// This should disconnect any connected device de-initialize the shared memory
	mem.write32(messagePointer, IPC::responseHeader(0x2, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void IRUserService::getConnectionStatusEvent(u32 messagePointer) {
	log("IR:USER: GetConnectionStatusEvent\n");

	if (!connectionStatusEvent.has_value()) {
		connectionStatusEvent = kernel.makeEvent(ResetType::OneShot);
	}

	mem.write32(messagePointer, IPC::responseHeader(0xC, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	// TOOD: Descriptor here
	mem.write32(messagePointer + 12, connectionStatusEvent.value());
}

void IRUserService::getReceiveEvent(u32 messagePointer) {
	log("IR:USER: GetReceiveEvent\n");

	if (!receiveEvent.has_value()) {
		receiveEvent = kernel.makeEvent(ResetType::OneShot);
	}

	mem.write32(messagePointer, IPC::responseHeader(0xA, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0x40000000);
	// TOOD: Descriptor here
	mem.write32(messagePointer + 12, receiveEvent.value());
}

void IRUserService::getSendEvent(u32 messagePointer) {
	log("IR:USER: GetSendEvent\n");

	if (!sendEvent.has_value()) {
		sendEvent = kernel.makeEvent(ResetType::OneShot);
	}

	mem.write32(messagePointer, IPC::responseHeader(0xB, 1, 2));
	mem.write32(messagePointer + 4, Result::Success);
	mem.write32(messagePointer + 8, 0x40000000);
	// TOOD: Descriptor here
	mem.write32(messagePointer + 12, sendEvent.value());
}

void IRUserService::requireConnection(u32 messagePointer) {
	const u8 deviceID = mem.read8(messagePointer + 4);
	log("IR:USER: RequireConnection (device: %d)\n", deviceID);

	// Reference: https://github.com/citra-emu/citra/blob/c10ffda91feb3476a861c47fb38641c1007b9d33/src/core/hle/service/ir/ir_user.cpp#L306
	if (sharedMemory.has_value()) {
		u32 sharedMemAddress = sharedMemory.value().addr;

		if (deviceID == u8(DeviceID::CirclePadPro)) {
			const bool enableCirclePadPro = config.circlePadProEnabled;  // TODO: This should always be true for N3DS too

			// Note: We temporarily pretend we don't have a CirclePad Pro. This code must change when we emulate it or N3DS C-stick
			const u8 status = (enableCirclePadPro) ? 2 : 1;  // Any value other than 2 is considered not connected.
			const u8 role = (enableCirclePadPro) ? 2 : 0;
			const u8 connected = (enableCirclePadPro) ? 1 : 0;

			if (enableCirclePadPro) {
				cpp.connect();
				// Slight hack: For some reason, CirclePad Pro breaks in some games (eg Majora's Mask 3D) unless we do this
				if (receiveEvent) {
					kernel.signalEvent(*receiveEvent);
				}
			}

			mem.write8(sharedMemAddress + offsetof(SharedMemoryStatus, connectionStatus), status);
			mem.write8(sharedMemAddress + offsetof(SharedMemoryStatus, connectionRole), role);
			mem.write8(sharedMemAddress + offsetof(SharedMemoryStatus, isConnected), connected);

			connectedDevice = connected;
			if (connectionStatusEvent.has_value()) {
				kernel.signalEvent(connectionStatusEvent.value());
			}
		} else {
			log("IR:USER: Unknown device %d\n", deviceID);
			mem.write8(sharedMemAddress + offsetof(SharedMemoryStatus, connectionStatus), 1);
			mem.write8(sharedMemAddress + offsetof(SharedMemoryStatus, connectionAttemptStatus), 2);
		}
	} else {
		Helpers::warn("RequireConnection without shmem");
	}

	mem.write32(messagePointer, IPC::responseHeader(0x6, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void IRUserService::sendIrnop(u32 messagePointer) {
	const u32 bufferSize = mem.read32(messagePointer + 4);
	const u32 inputPointer = mem.read32(messagePointer + 12);

	std::vector<u8> data;
	data.reserve(bufferSize);

	for (u32 i = 0; i < bufferSize; i++) {
		data.push_back(mem.read8(inputPointer + i));
	}

	mem.write32(messagePointer, IPC::responseHeader(0xD, 1, 0));

	if (connectedDevice) {
		cpp.receivePayload(data);
		if (sendEvent.has_value()) {
			kernel.signalEvent(sendEvent.value());
		}

		mem.write32(messagePointer + 4, Result::Success);
	} else {
		Helpers::warn("IR:USER: SendIrnop without connected device");
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
	}
}

void IRUserService::disconnect(u32 messagePointer) {
	log("IR:USER: Disconnect\n");

	if (sharedMemory.has_value()) {
		u32 sharedMemAddress = sharedMemory->addr;

		mem.write8(sharedMemAddress + offsetof(SharedMemoryStatus, connectionStatus), 0);
		mem.write8(sharedMemAddress + offsetof(SharedMemoryStatus, isConnected), 0);
	}

	// If there's a connected device, disconnect it and trigger the status event
	if (connectedDevice) {
		connectedDevice = false;
		cpp.disconnect();

		if (connectionStatusEvent.has_value()) {
			kernel.signalEvent(connectionStatusEvent.value());
		}
	}

	mem.write32(messagePointer, IPC::responseHeader(0x9, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void IRUserService::releaseReceivedData(u32 messagePointer) {
	const u32 size = mem.read32(messagePointer + 4);
	log("IR:USER: ReleaseReceivedData (%08X)\n", size);

	mem.write32(messagePointer, IPC::responseHeader(0x19, 1, 0));

	if (receiveBuffer) {
		if (receiveBuffer->release(size)) {
			mem.write32(messagePointer + 4, Result::Success);
		} else {
			Helpers::warn("IR:USER: ReleaseReceivedData failed to release\n");
			mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		}
	} else {
		Helpers::warn("IR:USER: ReleaseReceivedData with no receive buffer?\n");
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
	}
}

void IRUserService::clearReceiveBuffer(u32 messagePointer) {
	log("IR:USER: ClearReceiveBuffer\n");
	mem.write32(messagePointer, IPC::responseHeader(0x0C, 1, 0));

	if (receiveBuffer) {
		if (receiveBuffer->release(receiveBuffer->getPacketCount())) {
			mem.write32(messagePointer + 4, Result::Success);
		} else {
			Helpers::warn("IR:USER: ClearReceiveBuffer failed to release\n");
			mem.write32(messagePointer + 4, Result::FailurePlaceholder);
		}
	} else {
		Helpers::warn("IR:USER: ClearReceiveBuffer with no receive buffer?\n");
		mem.write32(messagePointer + 4, Result::FailurePlaceholder);
	}
}

void IRUserService::clearSendBuffer(u32 messagePointer) {
	log("IR:USER: ClearSendBuffer (Stubbed)\n");

	mem.write32(messagePointer, IPC::responseHeader(0x0D, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void IRUserService::sendPayload(IRUserService::Payload payload) {
	if (!receiveBuffer) {
		return;
	}

	// Based on: https://github.com/azahar-emu/azahar/blob/df3c0c18e4b71ecb5c4e009bfc07b9fd14fd39d9/src/core/hle/service/ir/ir_user.cpp#L231
	std::vector<u8> packet;

	// Builds packet header. For the format info:
	// https://www.3dbrew.org/wiki/IRUSER_Shared_Memory#Packet_structure
	packet.push_back(0xA5);
	const u8 networkID = *(receiveBuffer->getPointer(offsetof(SharedMemoryStatus, networkID)));
	packet.push_back(networkID);

	// Append size info.
	// The highest bit of the first byte is unknown, which is set to zero here. The second highest
	// bit is a flag that determines whether the size info is in extended form. If the packet size
	// can be represent within 6 bits, the short form (1 byte) of size info is chosen, the size is
	// put to the lower bits of this byte, and the flag is clear. If the packet size cannot be
	// represent within 6 bits, the extended form (2 bytes) is chosen, the lower 8 bits of the size
	// is put to the second byte, the higher bits of the size is put to the lower bits of the first
	// byte, and the flag is set. Note that the packet size must be within 14 bits due to this
	// format restriction, or it will overlap with the flag bit.
	usize size = payload.size();
	if (size < 0x40) {
		packet.push_back(static_cast<u8>(size));
	} else if (size < 0x4000) {
		packet.push_back(static_cast<u8>(size >> 8) | 0x40);
		packet.push_back(static_cast<u8>(size));
	}

	// Insert the payload data + CRC8 checksum into the packet
	packet.insert(packet.end(), payload.begin(), payload.end());
	packet.push_back(crc8(packet));

	if (receiveBuffer->put(packet) && receiveEvent.has_value()) {
		kernel.signalEvent(receiveEvent.value());
	}
}

void IRUserService::updateCirclePadPro() {
	if (!connectedDevice || !receiveBuffer) {
		return;
	}

	// The button state for the CirclePad Pro is stored in the HID service to make the frontend logic simpler
	// We take the button state, format it nicely into the CirclePad Pro struct, and return it
	auto& cppState = cpp.state;
	u32 buttons = hid.getOldButtons();

	cppState.buttons.zlNotPressed = (buttons & HID::Keys::ZL) ? 0 : 1;
	cppState.buttons.zrNotPressed = (buttons & HID::Keys::ZR) ? 0 : 1;
	cppState.cStick.x = hid.getCStickX();
	cppState.cStick.y = hid.getCStickY();

	std::vector<u8> response(sizeof(cppState));
	std::memcpy(response.data(), &cppState, sizeof(cppState));
	sendPayload(response);

	// Schedule next IR event. TODO: Maybe account for cycle drift.
	auto& scheduler = kernel.getScheduler();
	scheduler.addEvent(Scheduler::EventType::UpdateIR, scheduler.currentTimestamp + cpp.period);
}