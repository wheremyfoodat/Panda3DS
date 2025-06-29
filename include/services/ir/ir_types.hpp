#pragma once
#include <array>

#include "bitfield.hpp"
#include "helpers.hpp"
#include "memory.hpp"
#include "swap.hpp"

// Type definitions for the IR service
// Based off https://github.com/azahar-emu/azahar/blob/de7b457ee466e432047c4ee8d37fca9eae7e031e/src/core/hle/service/ir/ir_user.cpp#L88

namespace IR {
	class Buffer {
	  public:
		Buffer(Memory& memory, u32 sharedMemBaseVaddr, u32 infoOffset, u32 bufferOffset, u32 maxPackets, u32 bufferSize)
			: memory(memory), sharedMemBaseVaddr(sharedMemBaseVaddr), infoOffset(infoOffset), bufferOffset(bufferOffset), maxPackets(maxPackets),
			  maxDataSize(bufferSize - sizeof(PacketInfo) * maxPackets) {
			updateBufferInfo();
		}

		u8* getPointer(u32 offset) { return (u8*)memory.getReadPointer(sharedMemBaseVaddr + offset); }

		bool put(std::span<const u8> packet) {
			if (info.packetCount == maxPackets) {
				return false;
			}

			u32 offset;

			// finds free space offset in data buffer
			if (info.packetCount == 0) {
				offset = 0;
				if (packet.size() > maxDataSize) {
					return false;
				}
			} else {
				const u32 lastIndex = (info.endIndex + maxPackets - 1) % maxPackets;
				const PacketInfo first = getPacketInfo(info.beginIndex);
				const PacketInfo last = getPacketInfo(lastIndex);
				offset = (last.offset + last.size) % maxDataSize;
				const u32 freeSpace = (first.offset + maxDataSize - offset) % maxDataSize;

				if (packet.size() > freeSpace) {
					return false;
				}
			}

			// Writes packet info
			PacketInfo packet_info{offset, static_cast<u32>(packet.size())};
			setPacketInfo(info.endIndex, packet_info);

			// Writes packet data
			for (std::size_t i = 0; i < packet.size(); ++i) {
				*getDataBufferPointer((offset + i) % maxDataSize) = packet[i];
			}

			// Updates buffer info
			info.endIndex++;
			info.endIndex %= maxPackets;
			info.packetCount++;
			updateBufferInfo();
			return true;
		}

		bool release(u32 count) {
			if (info.packetCount < count) {
				return false;
			}

			info.packetCount -= count;
			info.beginIndex += count;
			info.beginIndex %= maxPackets;
			updateBufferInfo();
			return true;
		}

		u32 getPacketCount() { return info.packetCount; }

	  private:
		struct BufferInfo {
			u32_le beginIndex;
			u32_le endIndex;
			u32_le packetCount;
			u32_le unknown;
		};
		static_assert(sizeof(BufferInfo) == 16, "IR::BufferInfo has wrong size!");

		struct PacketInfo {
			u32_le offset;
			u32_le size;
		};
		static_assert(sizeof(PacketInfo) == 8, "IR::PacketInfo has wrong size!");

		u8* getPacketInfoPointer(u32 index) { return getPointer(bufferOffset + sizeof(PacketInfo) * index); }
		void setPacketInfo(u32 index, const PacketInfo& packet_info) { std::memcpy(getPacketInfoPointer(index), &packet_info, sizeof(PacketInfo)); }

		PacketInfo getPacketInfo(u32 index) {
			PacketInfo packet_info;
			std::memcpy(&packet_info, getPacketInfoPointer(index), sizeof(PacketInfo));
			return packet_info;
		}

		u8* getDataBufferPointer(u32 offset) { return getPointer(bufferOffset + sizeof(PacketInfo) * maxPackets + offset); }
		void updateBufferInfo() {
			if (infoOffset) {
				std::memcpy(getPointer(infoOffset), &info, sizeof(info));
			}
		}

		BufferInfo info{0, 0, 0, 0};
		Memory& memory;
		u32 sharedMemBaseVaddr;
		u32 infoOffset;
		u32 bufferOffset;
		u32 maxPackets;
		u32 maxDataSize;
	};

	struct CPPResponse {
		union {
			BitField<0, 8, u32> header;
			BitField<8, 12, u32> c_stick_x;
			BitField<20, 12, u32> c_stick_y;
		} c_stick;
		union {
			BitField<0, 5, u8> battery_level;
			BitField<5, 1, u8> zl_not_held;
			BitField<6, 1, u8> zr_not_held;
			BitField<7, 1, u8> r_not_held;
		} buttons;
		u8 unknown;
	};
	static_assert(sizeof(CPPResponse) == 6, "Circlepad Pro response has wrong size");

	namespace CPPResponseID {
		enum : u8 {
			PollButtons = 0x10,          // This response contains the current Circlepad Pro button/stick state
			ReadCalibrationData = 0x11,  // Responding to a ReadCalibrationData request
		};
	}

	namespace CPPRequestID {
		enum : u8 {
			ConfigurePolling = 1,     // Configures if & how often to poll the Circlepad Pro button/stick state
			ReadCalibrationData = 2,  // Reads the Circlepad Pro calibration data
		};
	}
}  // namespace IR
