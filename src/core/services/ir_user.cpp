#include "services/ir_user.hpp"

#include <cstddef>

#include "ipc.hpp"
#include "kernel.hpp"

namespace IRUserCommands {
	enum : u32 {
		FinalizeIrnop = 0x00020000,
		RequireConnection = 0x00060040,
		Disconnect = 0x00090000,
		GetReceiveEvent = 0x000A0000,
		GetConnectionStatusEvent = 0x000C0000,
		SendIrnop = 0x000D0042, 
		InitializeIrnopShared = 0x00180182
	};
}

void IRUserService::reset() {
	connectionStatusEvent = std::nullopt;
	receiveEvent = std::nullopt;
	sharedMemory = std::nullopt;
	connectedDevice = false;
}

void IRUserService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case IRUserCommands::Disconnect: disconnect(messagePointer); break;
		case IRUserCommands::FinalizeIrnop: finalizeIrnop(messagePointer); break;
		case IRUserCommands::GetReceiveEvent: getReceiveEvent(messagePointer); break;
		case IRUserCommands::GetConnectionStatusEvent: getConnectionStatusEvent(messagePointer); break;
		case IRUserCommands::InitializeIrnopShared: initializeIrnopShared(messagePointer); break;
		case IRUserCommands::RequireConnection: requireConnection(messagePointer); break;
		case IRUserCommands::SendIrnop: sendIrnop(messagePointer); break;
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

	log("IR:USER: InitializeIrnopShared (shared mem size = %08X, sharedMemHandle = %X) (stubbed)\n", sharedMemSize, sharedMemHandle);
	Helpers::warn("Game is initializing IR:USER. If it explodes, this is probably why");

	KernelObject* object = kernel.getObject(sharedMemHandle, KernelObjectType::MemoryBlock);
	if (object == nullptr) {
		Helpers::panic("IR::InitializeIrnopShared: Shared memory object does not exist");
	}

	MemoryBlock* memoryBlock = object->getData<MemoryBlock>();
	sharedMemory = *memoryBlock;

	// Set the initialized byte in shared mem to 1
	mem.write8(memoryBlock->addr + offsetof(SharedMemoryStatus, isInitialized), 1);
	mem.write64(memoryBlock->addr + 0x10, 0); // Initialize the receive buffer info to all 0s
	mem.write64(memoryBlock->addr + 0x18, 0);

	mem.write32(messagePointer, IPC::responseHeader(0x18, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void IRUserService::finalizeIrnop(u32 messagePointer) {
	log("IR:USER: FinalizeIrnop\n");

	if (connectedDevice) {
		connectedDevice = false;
		// This should also disconnect CirclePad Pro?
	}

	sharedMemory = std::nullopt;

	// This should disconnect any connected device de-initialize the shared memory
	mem.write32(messagePointer, IPC::responseHeader(0x2, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void IRUserService::getConnectionStatusEvent(u32 messagePointer) {
	log("IR:USER: GetConnectionStatusEvent\n");

	if (!connectionStatusEvent.has_value()) {
		connectionStatusEvent = kernel.makeEvent(ResetType::OneShot);
	}
	//kernel.signalEvent(connectionStatusEvent.value()); // ??????????????

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

void IRUserService::requireConnection(u32 messagePointer) {
	const u8 deviceID = mem.read8(messagePointer + 4);
	log("IR:USER: RequireConnection (device: %d)\n", deviceID);

	// Reference: https://github.com/citra-emu/citra/blob/c10ffda91feb3476a861c47fb38641c1007b9d33/src/core/hle/service/ir/ir_user.cpp#L306
	if (sharedMemory.has_value()) {
		u32 sharedMemAddress = sharedMemory.value().addr;

		if (deviceID == u8(DeviceID::CirclePadPro)) {
			// Note: We temporarily pretend we don't have a CirclePad Pro. This code must change when we emulate it or N3DS C-stick
			constexpr u8 status = 1; // Not connected. Any value other than 2 is considered not connected.
			constexpr u8 role = 0;
			constexpr u8 connected = 0;

			mem.write8(sharedMemAddress + offsetof(SharedMemoryStatus, connectionStatus), status);
			mem.write8(sharedMemAddress + offsetof(SharedMemoryStatus, connectionRole), role);
			mem.write8(sharedMemAddress + offsetof(SharedMemoryStatus, isConnected), connected);

			connectedDevice = true;
			if (connectionStatusEvent.has_value()) {
				kernel.signalEvent(connectionStatusEvent.value());
			}
		} else {
			log("IR:USER: Unknown device %d\n", deviceID);
			mem.write8(sharedMemAddress + offsetof(SharedMemoryStatus, connectionStatus), 1);
			mem.write8(sharedMemAddress + offsetof(SharedMemoryStatus, connectionAttemptStatus), 2);
		}
	}

	mem.write32(messagePointer, IPC::responseHeader(0x6, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

void IRUserService::sendIrnop(u32 messagePointer) {
	Helpers::panic("IR:USER: SendIrnop\n");

	mem.write32(messagePointer + 4, Result::Success);
}

void IRUserService::disconnect(u32 messagePointer) {
	log("IR:USER: Disconnect\n");

	if (sharedMemory.has_value()) {
		u32 sharedMemAddress = sharedMemory.value().addr;

		mem.write8(sharedMemAddress + offsetof(SharedMemoryStatus, connectionStatus), 0);
		mem.write8(sharedMemAddress + offsetof(SharedMemoryStatus, isConnected), 0);
	}

	// If there's a connected device, disconnect it and trigger the status event
	if (connectedDevice) {
		connectedDevice = false;
		if (connectionStatusEvent.has_value()) {
			kernel.signalEvent(connectionStatusEvent.value());
		}
	}

	mem.write32(messagePointer, IPC::responseHeader(0x9, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}