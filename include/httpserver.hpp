#ifdef PANDA3DS_ENABLE_HTTP_SERVER
#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include "helpers.hpp"

enum class HttpActionType { None, Screenshot, Key, TogglePause, Reset, LoadRom };

class Emulator;
namespace httplib {
	class Server;
	class Response;
}

// Wrapper for httplib::Response that allows the HTTP server to wait for the response to be ready
struct DeferredResponseWrapper {
	DeferredResponseWrapper(httplib::Response& response) : inner_response(response) {}

	httplib::Response& inner_response;
	std::mutex mutex;
	std::condition_variable cv;
	bool ready = false;
};

// Actions derive from this class and are used to communicate with the HTTP server
class HttpAction {
	HttpActionType type;

  public:
	HttpAction(HttpActionType type) : type(type) {}
	virtual ~HttpAction() = default;

	HttpActionType getType() const { return type; }

	static std::unique_ptr<HttpAction> createScreenshotAction(DeferredResponseWrapper& response);
	static std::unique_ptr<HttpAction> createKeyAction(uint32_t key, bool state);
	static std::unique_ptr<HttpAction> createLoadRomAction(std::filesystem::path path, bool paused);
	static std::unique_ptr<HttpAction> createTogglePauseAction();
	static std::unique_ptr<HttpAction> createResetAction();
};

struct HttpServer {
	HttpServer(Emulator* emulator);
	~HttpServer();
	void processActions();

  private:
	static constexpr const char* httpServerScreenshotPath = "screenshot.png";

	Emulator* emulator;
	std::unique_ptr<httplib::Server> server;

	std::thread httpServerThread;
	std::queue<std::unique_ptr<HttpAction>> actionQueue;
	std::mutex actionQueueMutex;

	std::map<std::string, u32> keyMap;
	bool paused = false;

	void startHttpServer();
	void pushAction(std::unique_ptr<HttpAction> action);
	std::string status();
	u32 stringToKey(const std::string& key_name);

	HttpServer(const HttpServer&) = delete;
	HttpServer& operator=(const HttpServer&) = delete;
};

#endif  // PANDA3DS_ENABLE_HTTP_SERVER