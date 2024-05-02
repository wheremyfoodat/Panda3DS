#include "panda_sdl/frontend_sdl.hpp"

#include <string>
#include <chrono>
#include <cstdio>
#include <thread>
#include "buttplugclient.h"

void callbackFunction(const mhl::Messages msg) {
	switch (msg.messageType) {
		case mhl::MessageTypes::DeviceList: printf("Device List callback\n"); break;
		case mhl::MessageTypes::DeviceAdded: printf("Device List callback\n"); break;
		case mhl::MessageTypes::ServerInfo: printf("Server info callback\n"); break;
		case mhl::MessageTypes::DeviceRemoved: printf("Device Removed callback\n"); break;
		case mhl::MessageTypes::SensorReading: printf("Sensor reading callback\n"); break;
		default: printf("Unknown message");
	}
}

int main(int argc, char *argv[]) {
	FrontendSDL app;
	std::string url = "ws://127.0.0.1";
	Client client(url, 12345, "test.txt");
	client.connect(callbackFunction);
	client.requestDeviceList();

	while (1) {
		client.startScan();

		std::vector<DeviceClass> myDevices = client.getDevices();
		if (myDevices.size() > 0) {
			client.sendScalar(myDevices[0], 0.5);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		client.stopScan();
		break;
	}

	if (argc > 1) {
		auto romPath = std::filesystem::current_path() / argv[1];
		if (!app.loadROM(romPath)) {
			// For some reason just .c_str() doesn't show the proper path
			Helpers::panic("Failed to load ROM file: %s", romPath.string().c_str());
		}
	} else {
		printf("No ROM inserted! Load a ROM by dragging and dropping it into the emulator window!\n");
	}

	app.run();
}
