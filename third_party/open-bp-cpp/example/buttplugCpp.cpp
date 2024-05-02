// buttplugCpp.cpp : Defines the entry point for the application.
//

#include "buttplugCpp.h"


using namespace std;

void callbackFunction(const mhl::Messages msg) {
	if (msg.messageType == mhl::MessageTypes::DeviceList) {
		cout << "Device List callback" << endl;
	}
	if (msg.messageType == mhl::MessageTypes::DeviceAdded) {
		cout << "Device Added callback" << endl;
	}
	if (msg.messageType == mhl::MessageTypes::ServerInfo) {
		cout << "Server Info callback" << endl;
	}
	if (msg.messageType == mhl::MessageTypes::DeviceRemoved) {
		cout << "Device Removed callback" << endl;
	}
	if (msg.messageType == mhl::MessageTypes::SensorReading) {
		cout << "Sensor Reading callback" << endl;
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

