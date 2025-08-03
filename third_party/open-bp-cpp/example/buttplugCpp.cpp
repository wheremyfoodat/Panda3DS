// buttplugCpp.cpp : Defines the entry point for the application.
//

#include "buttplugCpp.h"
#include <cstdio>

void callbackFunction(const mhl::Messages msg) {
	switch (msg.messageType) {
		case mhl::MessageTypes::DeviceList: std::printf("Device List callback\n"); break;
		case mhl::MessageTypes::DeviceAdded: std::printf("Device List callback\n"); break;
		case mhl::MessageTypes::ServerInfo: std::printf("Server info callback\n"); break;
		case mhl::MessageTypes::DeviceRemoved: std::printf("Device Removed callback\n"); break;
		case mhl::MessageTypes::SensorReading: std::printf("Sensor reading callback\n"); break;
		default: printf("Unknown message")
	}
}

int main()
{
	std::string url = "ws://127.0.0.1";
    std::cout << "\n";
	Client client(url, 12345, "test.txt");
	client.connect(callbackFunction);
	client.requestDeviceList();
	client.startScan();
	while (1) {
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		client.stopScan();
		break;
	}
	//std::vector<DeviceClass> myDevices = client.getDevices();
	//client.sendScalar(myDevices[0], 0.5);
	//client.sendScalar(myDevices[1], 0.5);
	//client.sensorSubscribe(myDevices[0], 0);
	//std::this_thread::sleep_for(std::chrono::milliseconds(20000));
	//client.sensorUnsubscribe(myDevices[0], 0);
	//client.stopAllDevices();
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	return 0;
}

