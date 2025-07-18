#pragma once
#include <functional>
#include <span>

#include "helpers.hpp"
#include "scheduler.hpp"

namespace IR {
	class Device {
	  public:
		using Payload = std::span<const u8>;

	  protected:
		using SendCallback = std::function<void(Payload)>;  // Callback for sending data from IR device->3DS
		Scheduler& scheduler;
		SendCallback sendCallback;

	  public:
		virtual void connect() = 0;
		virtual void disconnect() = 0;
		virtual void receivePayload(Payload payload) = 0;

		Device(SendCallback sendCallback, Scheduler& scheduler) : sendCallback(sendCallback), scheduler(scheduler) {}
	};
}  // namespace IR