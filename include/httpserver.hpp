#pragma once

#include <atomic>
#include <mutex>
#include "helpers.hpp"

enum class HttpAction { None, Screenshot, PressKey, ReleaseKey };

constexpr const char* httpServerScreenshotPath = "screenshot.png";

struct HttpServer
{
    std::atomic_bool pendingAction = false;
	HttpAction action = HttpAction::None;
	std::mutex actionMutex = {};
	u32 pendingKey = 0;

	void startHttpServer();
};