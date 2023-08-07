#ifdef PANDA3DS_ENABLE_DISCORD_RPC

#include "discord_rpc.hpp"

#include <cstring>
#include <ctime>

void Discord::RPC::init() {
	DiscordEventHandlers handlers{};
	Discord_Initialize("1138176975865909360", &handlers, 1, nullptr);

	startTimestamp = time(nullptr);
	enabled = true;
}

void Discord::RPC::update(Discord::RPCStatus status, const std::string& game) {
	DiscordRichPresence rpc{};

	if (status == Discord::RPCStatus::Playing) {
		rpc.details = "Playing a game";
		rpc.state = game.c_str();
	} else {
		rpc.details = "Idle";
	}

	rpc.largeImageKey = "pand";
	rpc.largeImageText = "Panda3DS is a 3DS emulator for Windows, MacOS and Linux";
	rpc.startTimestamp = startTimestamp;

	Discord_UpdatePresence(&rpc);
}

void Discord::RPC::stop() {
	if (enabled) {
		enabled = false;
		Discord_ClearPresence();
		Discord_Shutdown();
	}
}

#endif