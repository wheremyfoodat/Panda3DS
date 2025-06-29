#pragma once
#include "services/ir/ir_device.hpp"
#include "services/ir/ir_types.hpp"

namespace IR {
	class CirclePadPro : public IR::Device {
	  public:
		virtual void connect() override;
		virtual void disconnect() override;
		virtual void receivePayload(Payload payload) override;

		CirclePadPro(SendCallback sendCallback) : IR::Device(sendCallback) {}
	};
}  // namespace IR