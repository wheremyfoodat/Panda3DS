#pragma once

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "buttplugclient.h"

class ExternalHapticsManager {
	Client client;
	std::vector<DeviceClass> devices;
	SensorClass sensors;

  public:
	ExternalHapticsManager() : client("ws://127.0.0.1", 12345) {}

	void connect() {
		client.connect([](const mhl::Messages message) {
			switch (message.messageType) {
				case mhl::MessageTypes::DeviceList: std::printf("Device List callback\n"); break;
				case mhl::MessageTypes::DeviceAdded: std::printf("Device List callback\n"); break;
				case mhl::MessageTypes::ServerInfo: std::printf("Server info callback\n"); break;
				case mhl::MessageTypes::DeviceRemoved: std::printf("Device Removed callback\n"); break;
				case mhl::MessageTypes::SensorReading: std::printf("Sensor reading callback\n"); break;
				default: std::printf("Unknown message");
			}
		});
	}

	void requestDeviceList() { client.requestDeviceList(); }
	void startScan() { client.startScan(); }
	void stopScan() { client.stopScan(); }
	void stopAllDevices() { client.stopAllDevices(); }

	void getDevices() { devices = client.getDevices(); }
	void getSensors() { sensors = client.getSensors(); }

	void sendScalar(int index, double value) {
		if (index < devices.size()) {
			client.sendScalar(devices[index], value);
		}
	}

	void stopDevice(int index) {
		if (index < devices.size()) {
			client.stopDevice(devices[index]);
		}
	}
};