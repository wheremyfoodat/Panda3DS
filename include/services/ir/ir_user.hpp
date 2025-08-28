#pragma once
#include <memory>
#include <optional>
#include <span>

#include "config.hpp"
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "result/result.hpp"
#include "services/hid.hpp"
#include "services/ir/circlepad_pro.hpp"

// Circular dependencies in this project? Never
class Kernel;

class IRUserService {
	using Payload = IR::Device::Payload;
	using Handle = HorizonHandle;

	enum class DeviceID : u8 {
		CirclePadPro = 1,
	};

	Handle handle = KernelHandles::IR_USER;
	Memory& mem;
	Kernel& kernel;
	// The IR service has a reference to the HID service as that's where the frontends store CirclePad Pro button state in
	HIDService& hid;
	const EmulatorConfig& config;

	MAKE_LOG_FUNCTION(log, irUserLogger)

	// Service commands
	void clearReceiveBuffer(u32 messagePointer);
	void clearSendBuffer(u32 messagePointer);
	void disconnect(u32 messagePointer);
	void finalizeIrnop(u32 messagePointer);
	void getConnectionStatusEvent(u32 messagePointer);
	void getReceiveEvent(u32 messagePointer);
	void getSendEvent(u32 messagePointer);
	void initializeIrnopShared(u32 messagePointer);
	void requireConnection(u32 messagePointer);
	void sendIrnop(u32 messagePointer);
	void releaseReceivedData(u32 messagePointer);

	using IREvent = std::optional<Handle>;

	IREvent connectionStatusEvent = std::nullopt;
	IREvent receiveEvent = std::nullopt;
	IREvent sendEvent = std::nullopt;
	IR::CirclePadPro cpp;

	std::optional<MemoryBlock> sharedMemory = std::nullopt;
	std::unique_ptr<IR::Buffer> receiveBuffer;
	bool connectedDevice = false;

	// Header of the IR shared memory containing various bits of info
	// https://www.3dbrew.org/wiki/IRUSER_Shared_Memory
	struct SharedMemoryStatus {
		u32 latestReceiveError;
		u32 latestSharedError;

		u8 connectionStatus;
		u8 connectionAttemptStatus;
		u8 connectionRole;
		u8 machineID;
		u8 isConnected;
		u8 networkID;
		u8 isInitialized;  // https://github.com/citra-emu/citra/blob/c10ffda91feb3476a861c47fb38641c1007b9d33/src/core/hle/service/ir/ir_user.cpp#L41
		u8 unk1;
	};
	static_assert(sizeof(SharedMemoryStatus) == 16);

	// The IR service uses CRC8 with generator polynomial = 0x07 for verifying packets received from IR devices
	static u8 crc8(std::span<const u8> data);

	// IR devices call this to send a device->console payload
	void sendPayload(Payload payload);

  public:
	IRUserService(Memory& mem, HIDService& hid, const EmulatorConfig& config, Kernel& kernel);

	void setCStickX(s16 value) { cpp.state.cStick.x = value; }
	void setCStickY(s16 value) { cpp.state.cStick.y = value; }

	void reset();
	void handleSyncRequest(u32 messagePointer);
	void updateCirclePadPro();
};