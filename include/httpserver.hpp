#ifdef PANDA3DS_ENABLE_HTTP_SERVER
#pragma once

#include <array>
#include <atomic>
#include <map>
#include <mutex>

#include "helpers.hpp"

enum class HttpAction { None, Screenshot, PressKey, ReleaseKey };

struct HttpServer {
	static constexpr const char* httpServerScreenshotPath = "screenshot.png";

	std::atomic_bool pendingAction = false;
	HttpAction action = HttpAction::None;
	std::mutex actionMutex = {};
	u32 pendingKey = 0;

	HttpServer();

	void startHttpServer();
	std::string status();

private:
	std::map<std::string, std::pair<u32, bool>> keyMap;
	std::array<bool, 12> pressedKeys = {};
	bool paused = false;

	u32 stringToKey(const std::string& key_name);
	bool getKeyState(const std::string& key_name);
	void setKeyState(const std::string& key_name, bool state);
};

#endif  // PANDA3DS_ENABLE_HTTP_SERVER