#ifdef PANDA3DS_ENABLE_HTTP_SERVER
#pragma once

#include <atomic>
#include <mutex>

#include "helpers.hpp"

enum class HttpAction { None, Screenshot, PressKey, ReleaseKey };

struct HttpServer {
	static constexpr const char* httpServerScreenshotPath = "screenshot.png";

	std::atomic_bool pendingAction = false;
	HttpAction action = HttpAction::None;
	std::mutex actionMutex = {};
	u32 pendingKey = 0;

	void startHttpServer();
};

#endif  // PANDA3DS_ENABLE_HTTP_SERVER