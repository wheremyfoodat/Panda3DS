#include<string>
#include<map>

#include "messages.h"

namespace mhl {
	// Enum class for message types for convenience.
	enum class MessageTypes {
		Ok,
		Error,
		Ping,
		RequestServerInfo,
		ServerInfo,
		StartScanning,
		StopScanning,
		ScanningFinished,
		RequestDeviceList,
		DeviceList,
		DeviceAdded,
		DeviceRemoved,
		StopDeviceCmd,
		StopAllDevices,
		ScalarCmd,
		LinearCmd,
		RotateCmd,
		SensorReadCmd,
		SensorReading,
		SensorSubscribeCmd,
		SensorUnsubscribeCmd
	};

	typedef std::map<MessageTypes, std::string> MessageMap_t;

	// Class for request messages.
	class Requests {
	public:
		msg::RequestServerInfo requestServerInfo;
		msg::StartScanning startScanning;
		msg::StopScanning stopScanning;
		msg::ScanningFinished scanningFinished;
		msg::RequestDeviceList requestDeviceList;
		msg::DeviceRemoved deviceRemoved;
		msg::StopDeviceCmd stopDeviceCmd;
		msg::StopAllDevices stopAllDevices;
		msg::ScalarCmd scalarCmd;
		msg::SensorReadCmd sensorReadCmd;
		msg::SensorSubscribeCmd sensorSubscribeCmd;
		msg::SensorUnsubscribeCmd sensorUnsubscribeCmd;
	};

	// Class for messages received and for handling all types of messages.
	class Messages {
	public:
		MessageTypes messageType = MessageTypes::Ok;
		unsigned int Id;

		// A map for message strings corresponding to enum. This is in this class since it is more convenient.
		MessageMap_t messageMap = {
			{MessageTypes::Ok, "Ok"},
			{MessageTypes::Error, "Error"},
			{MessageTypes::Ping, "Ping"},
			{MessageTypes::RequestServerInfo, "RequestServerInfo"},
			{MessageTypes::ServerInfo, "ServerInfo"},
			{MessageTypes::StartScanning, "StartScanning"},
			{MessageTypes::StopScanning, "StopScanning"},
			{MessageTypes::ScanningFinished, "ScanningFinished"},
			{MessageTypes::RequestDeviceList, "RequestDeviceList"},
			{MessageTypes::DeviceList, "DeviceList"},
			{MessageTypes::DeviceAdded, "DeviceAdded"},
			{MessageTypes::DeviceRemoved, "DeviceRemoved"},
			{MessageTypes::StopDeviceCmd, "StopDeviceCmd"},
			{MessageTypes::StopAllDevices, "StopAllDevices"},
			{MessageTypes::ScalarCmd, "ScalarCmd"},
			{MessageTypes::LinearCmd, "LinearCmd"},
			{MessageTypes::RotateCmd, "RotateCmd"},
			{MessageTypes::SensorReadCmd, "SensorReadCmd"},
			{MessageTypes::SensorReading, "SensorReading"},
			{MessageTypes::SensorSubscribeCmd, "SensorSubscribeCmd"},
			{MessageTypes::SensorUnsubscribeCmd, "SensorUnsubscribeCmd"}
		};

		msg::Ok ok;
		msg::Error error;
		msg::ServerInfo serverInfo;
		msg::DeviceList deviceList;
		msg::DeviceAdded deviceAdded;
		msg::DeviceRemoved deviceRemoved;
		msg::SensorReading sensorReading;

		// Both server message and requests are handled in this class.
		void handleServerMessage(json& msg);
		json handleClientRequest(Requests req);
	private:
	};
}