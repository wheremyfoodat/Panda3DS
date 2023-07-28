#ifdef PANDA3DS_ENABLE_HTTP_SERVER
#include "http_server.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
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

class HttpActionLoadRom : public HttpAction {
	DeferredResponseWrapper& response;
	const std::filesystem::path& path;
	bool paused;

  public:
	HttpActionLoadRom(DeferredResponseWrapper& response, const std::filesystem::path& path, bool paused)
		: HttpAction(HttpActionType::LoadRom), response(response), path(path), paused(paused) {}

	DeferredResponseWrapper& getResponse() { return response; }
	const std::filesystem::path& getPath() const { return path; }
	bool getPaused() const { return paused; }
};

class HttpActionStep : public HttpAction {
	DeferredResponseWrapper& response;
	int frames;

  public:
	HttpActionStep(DeferredResponseWrapper& response, int frames)
		: HttpAction(HttpActionType::Step), response(response), frames(frames) {}

	DeferredResponseWrapper& getResponse() { return response; }
	int getFrames() const { return frames; }
};

std::unique_ptr<HttpAction> HttpAction::createScreenshotAction(DeferredResponseWrapper& response) {
	return std::make_unique<HttpActionScreenshot>(response);
}

std::unique_ptr<HttpAction> HttpAction::createKeyAction(u32 key, bool state) { return std::make_unique<HttpActionKey>(key, state); }
std::unique_ptr<HttpAction> HttpAction::createTogglePauseAction() { return std::make_unique<HttpActionTogglePause>(); }
std::unique_ptr<HttpAction> HttpAction::createResetAction() { return std::make_unique<HttpActionReset>(); }

std::unique_ptr<HttpAction> HttpAction::createLoadRomAction(DeferredResponseWrapper& response, const std::filesystem::path& path, bool paused) {
	return std::make_unique<HttpActionLoadRom>(response, path, paused);
}

std::unique_ptr<HttpAction> HttpAction::createStepAction(DeferredResponseWrapper& response, int frames) {
	return std::make_unique<HttpActionStep>(response, frames);
}

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
	server->set_tcp_nodelay(true);
	server->Get("/ping", [](const httplib::Request&, httplib::Response& response) { response.set_content("pong", "text/plain"); });

	server->Get("/screen", [this](const httplib::Request&, httplib::Response& response) {
		// TODO: make the below a DeferredResponseWrapper function
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

	server->Get("/step", [this](const httplib::Request& request, httplib::Response& response) {
		auto it = request.params.find("frames");
		if (it == request.params.end()) {
			response.set_content("error", "text/plain");
			return;
		}

		int frames;
		try {
			frames = std::stoi(it->second);
		} catch (...) {
			response.set_content("error", "text/plain");
			return;
		}

		if (frames <= 0) {
			response.set_content("error", "text/plain");
			return;
		}

		DeferredResponseWrapper wrapper(response);
		std::unique_lock lock(wrapper.mutex);
		pushAction(HttpAction::createStepAction(wrapper, frames));
		wrapper.cv.wait(lock, [&wrapper] { return wrapper.ready; });
	});

	server->Get("/status", [this](const httplib::Request&, httplib::Response& response) { response.set_content(status(), "text/plain"); });

	server->Get("/load_rom", [this](const httplib::Request& request, httplib::Response& response) {
		auto it = request.params.find("path");
		if (it == request.params.end()) {
			response.set_content("error", "text/plain");
			return;
		}

		std::filesystem::path romPath = it->second;
		if (romPath.empty()) {
			response.set_content("error", "text/plain");
			return;
		} else {
			std::error_code error;
			if (!std::filesystem::is_regular_file(romPath, error)) {
				std::string message = "error: " + error.message();
				response.set_content(message, "text/plain");
				return;
			}
		}

		bool paused = false;
		it = request.params.find("paused");
		if (it != request.params.end()) {
			paused = (it->second == "1");
		}

		DeferredResponseWrapper wrapper(response);
		std::unique_lock lock(wrapper.mutex);
		pushAction(HttpAction::createLoadRomAction(wrapper, romPath, paused));
		wrapper.cv.wait(lock, [&wrapper] { return wrapper.ready; });
	});

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

	if (framesToRun > 0) {
		if (!currentStepAction) {
			// Should never happen
			printf("framesToRun > 0 but no currentStepAction\n");
			return;
		}

		emulator->resume();
		framesToRun--;

		if (framesToRun == 0) {
			paused = true;
			emulator->pause();

			DeferredResponseWrapper& response = reinterpret_cast<HttpActionStep*>(currentStepAction.get())->getResponse();
			response.inner_response.set_content("ok", "text/plain");
			std::unique_lock<std::mutex> lock(response.mutex);
			response.ready = true;
			response.cv.notify_one();
		}
		
		// Don't process more actions until we're done stepping
		return;
	}

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

			case HttpActionType::LoadRom: {
				HttpActionLoadRom* loadRomAction = static_cast<HttpActionLoadRom*>(action.get());
				DeferredResponseWrapper& response = loadRomAction->getResponse();
				bool loaded = emulator->loadROM(loadRomAction->getPath());

				response.inner_response.set_content(loaded ? "ok" : "error", "text/plain");

				std::unique_lock<std::mutex> lock(response.mutex);
				response.ready = true;
				response.cv.notify_one();

				if (loaded) {
					paused = loadRomAction->getPaused();
					framesToRun = 0;
					if (paused) {
						emulator->pause();
					} else {
						emulator->resume();
					}
				}
				break;
			}

			case HttpActionType::TogglePause:
				framesToRun = 0;
				emulator->togglePause();
				paused = !paused;
				break;

			case HttpActionType::Reset: emulator->reset(Emulator::ReloadOption::Reload); break;

			case HttpActionType::Step: {
				HttpActionStep* stepAction = static_cast<HttpActionStep*>(action.get());
				framesToRun = stepAction->getFrames();
				currentStepAction = std::move(action);
				break;
			}

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