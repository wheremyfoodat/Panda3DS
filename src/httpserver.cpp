#ifdef PANDA3DS_ENABLE_HTTP_SERVER
#include "httpserver.hpp"

#include <fstream>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "httplib.h"
#include "services/hid.hpp"

u32 stringToKey(const std::string& key_name) {
	namespace Keys = HID::Keys;
	static std::map<std::string, u32> keyMap = {
		{"A", Keys::A},
		{"B", Keys::B},
		{"Select", Keys::Select},
		{"Start", Keys::Start},
		{"Right", Keys::Right},
		{"Left", Keys::Left},
		{"Up", Keys::Up},
		{"Down", Keys::Down},
		{"R", Keys::R},
		{"L", Keys::L},
		{"X", Keys::X},
		{"Y", Keys::Y},
		{"CirclePadRight", Keys::CirclePadRight},
		{"CirclePadLeft", Keys::CirclePadLeft},
		{"CirclePadUp", Keys::CirclePadUp},
		{"CirclePadDown", Keys::CirclePadDown},
	};

	if (keyMap.find(key_name) != keyMap.end()) {
		return keyMap[key_name];
	}

	return 0;
}

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
					} else if (value == "0") {
						action = HttpAction::ReleaseKey;
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

		// TODO: ability to specify host and port
		printf("Starting HTTP server on port 1234\n");
		server.listen("localhost", 1234);
	});

	http_thread.detach();
}

#endif  // PANDA3DS_ENABLE_HTTP_SERVER