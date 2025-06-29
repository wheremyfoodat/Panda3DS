#pragma once
#include <functional>
#include <span>

#include "helpers.hpp"

namespace IR {
	class Device {
	  public:
		using Payload = std::span<const u8>;

	  protected:
		using SendCallback = std::function<void(Payload)>;  // Callback for sending data from IR device->3DS
		SendCallback sendCallback;

	  public:
		virtual void connect() = 0;
		virtual void disconnect() = 0;
		virtual void receivePayload(Payload payload) = 0;

		Device(SendCallback sendCallback) : sendCallback(sendCallback) {}
	};
}  // namespace IR