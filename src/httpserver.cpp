#ifdef PANDA3DS_ENABLE_HTTP_SERVER
#include "httpserver.hpp"

#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <sstream>

#include "httplib.h"
#include "services/hid.hpp"

HttpServer::HttpServer() : keyMap(
	{
		{"A", { HID::Keys::A, false } },
		{"B", { HID::Keys::B, false } },
		{"Select", { HID::Keys::Select, false } },
		{"Start", { HID::Keys::Start, false } },
		{"Right", { HID::Keys::Right, false } },
		{"Left", { HID::Keys::Left, false } },
		{"Up", { HID::Keys::Up, false } },
		{"Down", { HID::Keys::Down, false } },
		{"R", { HID::Keys::R, false } },
		{"L", { HID::Keys::L, false } },
		{"X", { HID::Keys::X, false } },
		{"Y", { HID::Keys::Y, false } },
	}
) {}

void HttpServer::startHttpServer() {
	std::thread http_thread([this]() {
		httplib::Server server;

		server.Get("/ping", [](const httplib::Request&, httplib::Response& response) { response.set_content("pong", "text/plain"); });

		server.Get("/screen", [this](const httplib::Request&, httplib::Response& response) {
			{
				std::scoped_lock lock(actionMutex);
				pendingAction = true;
				action = HttpAction::Screenshot;
			}
			// wait until the screenshot is ready
			pendingAction.wait(true);
			std::ifstream image(httpServerScreenshotPath, std::ios::binary);
			std::vector<char> buffer(std::istreambuf_iterator<char>(image), {});
			response.set_content(buffer.data(), buffer.size(), "image/png");
		});

		server.Get("/input", [this](const httplib::Request& request, httplib::Response& response) {
			bool ok = false;
			for (auto& [keyStr, value] : request.params) {
				auto key = stringToKey(keyStr);
				printf("Param: %s\n", keyStr.c_str());

				if (key != 0) {
					std::scoped_lock lock(actionMutex);
					pendingAction = true;
					pendingKey = key;
					ok = true;
					if (value == "1") {
						action = HttpAction::PressKey;
						setKeyState(keyStr, true);
					} else if (value == "0") {
						action = HttpAction::ReleaseKey;
						setKeyState(keyStr, false);
					} else {
						// Should not happen but just in case
						pendingAction = false;
						ok = false;
					}
					// Not supporting multiple keys at once for now (ever?)
					break;
				}
			}

			if (ok) {
				response.set_content("ok", "text/plain");
			}
		});

		server.Get("/step", [this](const httplib::Request&, httplib::Response& response) {
			// TODO: implement /step
			response.set_content("ok", "text/plain");
		});

		server.Get("/status", [this](const httplib::Request&, httplib::Response& response) {
			response.set_content(status(), "text/plain");
		});

		// TODO: ability to specify host and port
		printf("Starting HTTP server on port 1234\n");
		server.listen("localhost", 1234);
	});

	http_thread.detach();
}

std::string HttpServer::status() {
	std::stringstream stringStream;

	stringStream << "Panda3DS\n";
	stringStream << "Status: " << (paused ? "Paused" : "Running") << "\n";
	for (auto& [keyStr, value] : keyMap) {
		stringStream << keyStr << ": " << value.second << "\n";
	}

	return stringStream.str();
}

u32 HttpServer::stringToKey(const std::string& key_name) {
	if (keyMap.find(key_name) != keyMap.end()) {
		return keyMap[key_name].first;
	}

	return 0;
}

bool HttpServer::getKeyState(const std::string& key_name) {
	if (keyMap.find(key_name) != keyMap.end()) {
		return keyMap[key_name].second;
	}

	return false;
}

void HttpServer::setKeyState(const std::string& key_name, bool state) {
	if (keyMap.find(key_name) != keyMap.end()) {
		keyMap[key_name].second = state;
	}
}

#endif  // PANDA3DS_ENABLE_HTTP_SERVER