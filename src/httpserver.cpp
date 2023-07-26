#ifdef PANDA3DS_ENABLE_HTTP_SERVER
#include "httpserver.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "emulator.hpp"
#include "helpers.hpp"
#include "httplib.h"

class HttpActionScreenshot : public HttpAction {
	DeferredResponseWrapper& response;

  public:
	HttpActionScreenshot(DeferredResponseWrapper& response) : HttpAction(HttpActionType::Screenshot), response(response) {}
	DeferredResponseWrapper& getResponse() { return response; }
};

class HttpActionTogglePause : public HttpAction {
  public:
	HttpActionTogglePause() : HttpAction(HttpActionType::TogglePause) {}
};

class HttpActionReset : public HttpAction {
  public:
	HttpActionReset() : HttpAction(HttpActionType::Reset) {}
};

class HttpActionKey : public HttpAction {
	u32 key;
	bool state;

  public:
	HttpActionKey(u32 key, bool state) : HttpAction(HttpActionType::Key), key(key), state(state) {}

	u32 getKey() const { return key; }
	bool getState() const { return state; }
};

std::unique_ptr<HttpAction> HttpAction::createScreenshotAction(DeferredResponseWrapper& response) {
	return std::make_unique<HttpActionScreenshot>(response);
}

std::unique_ptr<HttpAction> HttpAction::createKeyAction(u32 key, bool state) { return std::make_unique<HttpActionKey>(key, state); }
std::unique_ptr<HttpAction> HttpAction::createTogglePauseAction() { return std::make_unique<HttpActionTogglePause>(); }
std::unique_ptr<HttpAction> HttpAction::createResetAction() { return std::make_unique<HttpActionReset>(); }

HttpServer::HttpServer(Emulator* emulator)
	: emulator(emulator), server(std::make_unique<httplib::Server>()), keyMap({
																		   {"A", {HID::Keys::A}},
																		   {"B", {HID::Keys::B}},
																		   {"Select", {HID::Keys::Select}},
																		   {"Start", {HID::Keys::Start}},
																		   {"Right", {HID::Keys::Right}},
																		   {"Left", {HID::Keys::Left}},
																		   {"Up", {HID::Keys::Up}},
																		   {"Down", {HID::Keys::Down}},
																		   {"R", {HID::Keys::R}},
																		   {"L", {HID::Keys::L}},
																		   {"X", {HID::Keys::X}},
																		   {"Y", {HID::Keys::Y}},
																	   }) {
	httpServerThread = std::thread(&HttpServer::startHttpServer, this);
}

HttpServer::~HttpServer() {
	printf("Stopping http server...\n");
	server->stop();
	if (httpServerThread.joinable()) {
		httpServerThread.join();
	}
}

void HttpServer::pushAction(std::unique_ptr<HttpAction> action) {
	std::scoped_lock lock(actionQueueMutex);
	actionQueue.push(std::move(action));
}

void HttpServer::startHttpServer() {
	server->Get("/ping", [](const httplib::Request&, httplib::Response& response) { response.set_content("pong", "text/plain"); });

	server->Get("/screen", [this](const httplib::Request&, httplib::Response& response) {
		DeferredResponseWrapper wrapper(response);
		// Lock the mutex before pushing the action to ensure that the condition variable is not notified before we wait on it
		std::unique_lock lock(wrapper.mutex);
		pushAction(HttpAction::createScreenshotAction(wrapper));
		wrapper.cv.wait(lock, [&wrapper] { return wrapper.ready; });
	});

	server->Get("/input", [this](const httplib::Request& request, httplib::Response& response) {
		bool ok = false;
		for (auto& [keyStr, value] : request.params) {
			u32 key = stringToKey(keyStr);

			if (key != 0) {
				bool state = (value == "1");
				if (!state && value != "0") {
					// Invalid state
					ok = false;
					break;
				}

				pushAction(HttpAction::createKeyAction(key, state));
				ok = true;
			} else {
				// Invalid key
				ok = false;
				break;
			}
		}

		response.set_content(ok ? "ok" : "error", "text/plain");
	});

	server->Get("/step", [this](const httplib::Request&, httplib::Response& response) {
		// TODO: implement /step
		response.set_content("ok", "text/plain");
	});

	server->Get("/status", [this](const httplib::Request&, httplib::Response& response) { response.set_content(status(), "text/plain"); });

	server->Get("/togglepause", [this](const httplib::Request&, httplib::Response& response) {
		pushAction(HttpAction::createTogglePauseAction());
		response.set_content("ok", "text/plain");
	});

	server->Get("/reset", [this](const httplib::Request&, httplib::Response& response) {
		pushAction(HttpAction::createResetAction());
		response.set_content("ok", "text/plain");
	});

	// TODO: ability to specify host and port
	printf("Starting HTTP server on port 1234\n");
	server->listen("localhost", 1234);
}

std::string HttpServer::status() {
	HIDService& hid = emulator->kernel.getServiceManager().getHID();
	std::stringstream stringStream;

	stringStream << "Panda3DS\n";
	stringStream << "Status: " << (paused ? "Paused" : "Running") << "\n";

	// TODO: This currently doesn't work for N3DS buttons
	auto keyPressed = [](const HIDService& hid, u32 mask) { return (hid.getOldButtons() & mask) != 0; };
	for (auto& [keyStr, value] : keyMap) {
		stringStream << keyStr << ": " << keyPressed(hid, value) << "\n";
	}

	return stringStream.str();
}

void HttpServer::processActions() {
	std::scoped_lock lock(actionQueueMutex);

	HIDService& hid = emulator->kernel.getServiceManager().getHID();

	while (!actionQueue.empty()) {
		std::unique_ptr<HttpAction> action = std::move(actionQueue.front());
		actionQueue.pop();

		switch (action->getType()) {
			case HttpActionType::Screenshot: {
				HttpActionScreenshot* screenshotAction = static_cast<HttpActionScreenshot*>(action.get());
				emulator->gpu.screenshot(httpServerScreenshotPath);
				std::ifstream file(httpServerScreenshotPath, std::ios::binary);
				std::vector<char> buffer(std::istreambuf_iterator<char>(file), {});

				DeferredResponseWrapper& response = screenshotAction->getResponse();
				response.inner_response.set_content(buffer.data(), buffer.size(), "image/png");
				std::unique_lock<std::mutex> lock(response.mutex);
				response.ready = true;
				response.cv.notify_one();
				break;
			}

			case HttpActionType::Key: {
				HttpActionKey* keyAction = static_cast<HttpActionKey*>(action.get());
				if (keyAction->getState()) {
					hid.pressKey(keyAction->getKey());
				} else {
					hid.releaseKey(keyAction->getKey());
				}
				break;
			}

			case HttpActionType::TogglePause: emulator->togglePause(); break;
			case HttpActionType::Reset: emulator->reset(Emulator::ReloadOption::Reload); break;

			default: break;
		}
	}
}

u32 HttpServer::stringToKey(const std::string& key_name) {
	if (keyMap.find(key_name) != keyMap.end()) {
		return keyMap[key_name];
	}

	return 0;
}

#endif  // PANDA3DS_ENABLE_HTTP_SERVER