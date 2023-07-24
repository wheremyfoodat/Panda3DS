#ifdef PANDA3DS_ENABLE_HTTP_SERVER
#include "httpserver.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "emulator.hpp"
#include "httplib.h"

class HttpActionScreenshot : public HttpAction {
  public:
	HttpActionScreenshot(DeferredResponseWrapper& response) : HttpAction(HttpActionType::Screenshot), response(response) {}

	DeferredResponseWrapper& getResponse() { return response; }

  private:
	DeferredResponseWrapper& response;
};

class HttpActionKey : public HttpAction {
  public:
	HttpActionKey(uint32_t key, bool state) : HttpAction(HttpActionType::Key), key(key), state(state) {}

	uint32_t getKey() const { return key; }
	bool getState() const { return state; }

  private:
	uint32_t key;
	bool state;
};

std::unique_ptr<HttpAction> HttpAction::createScreenshotAction(DeferredResponseWrapper& response) {
	return std::make_unique<HttpActionScreenshot>(response);
}

std::unique_ptr<HttpAction> HttpAction::createKeyAction(uint32_t key, bool state) { return std::make_unique<HttpActionKey>(key, state); }

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
				bool state = value == "1";

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

		if (ok) {
			response.set_content("ok", "text/plain");
		} else {
			response.set_content("error", "text/plain");
		}
	});

	server->Get("/step", [this](const httplib::Request&, httplib::Response& response) {
		// TODO: implement /step
		response.set_content("ok", "text/plain");
	});

	server->Get("/status", [this](const httplib::Request&, httplib::Response& response) { response.set_content(status(), "text/plain"); });

	// TODO: ability to specify host and port
	printf("Starting HTTP server on port 1234\n");
	server->listen("localhost", 1234);
}

std::string HttpServer::status() {
	HIDService& hid = emulator->kernel.getServiceManager().getHID();

	std::stringstream stringStream;

	stringStream << "Panda3DS\n";
	stringStream << "Status: " << (paused ? "Paused" : "Running") << "\n";

	for (auto& [keyStr, value] : keyMap) {
		stringStream << keyStr << ": " << hid.isPressed(value) << "\n";
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
			default: {
				break;
			}
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