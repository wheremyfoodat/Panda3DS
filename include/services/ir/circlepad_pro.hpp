#pragma once
#include "bitfield.hpp"
#include "services/ir/ir_device.hpp"
#include "services/ir/ir_types.hpp"

namespace IR {
	class CirclePadPro : public IR::Device {
	  public:
		struct ButtonState {
			static constexpr int C_STICK_CENTER = 0x800;
			static constexpr int C_STICK_RADIUS = 0x7FF;

			union {
				BitField<0, 8, u32> header;
				BitField<8, 12, u32> x;
				BitField<20, 12, u32> y;
			} cStick;

			union {
				BitField<0, 5, u8> batteryLevel;
				BitField<5, 1, u8> zlNotPressed;
				BitField<6, 1, u8> zrNotPressed;
				BitField<7, 1, u8> rNotPressed;
			} buttons;

			u8 unknown;

			ButtonState() {
				// Response header for button state reads
				cStick.header = static_cast<u8>(CPPResponseID::PollButtons);
				// Fully charged
				buttons.batteryLevel = 0x1F;
				unknown = 0;
			}
		};
		static_assert(sizeof(ButtonState) == 6, "Circlepad Pro button state has wrong size");

		virtual void connect() override;
		virtual void disconnect() override;
		virtual void receivePayload(Payload payload) override;

		CirclePadPro(SendCallback sendCallback) : IR::Device(sendCallback) {}
		ButtonState state;
	};
}  // namespace IR