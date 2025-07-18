#pragma once

#ifdef PANDA3DS_ENABLE_DISCORD_RPC
#include <discord_rpc.h>

#include <cstdint>
#include <string>

namespace Discord {
	enum class RPCStatus { Idling, Playing };

	class RPC {
		std::uint64_t startTimestamp;
		bool enabled = false;

	  public:
		void init();
		void update(RPCStatus status, const std::string& title);
		void stop();

		bool running() const { return enabled; }
	};
}  // namespace Discord

#endif