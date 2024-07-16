#include "../include/messages.h"

// Function definitions for json conversions.
namespace msg {
	void to_json(json& j, const RequestServerInfo& k) {
		j["RequestServerInfo"] = { {"Id", k.Id}, {"ClientName", k.ClientName}, {"MessageVersion", k.MessageVersion} };
	}

	void to_json(json& j, const StartScanning& k) {
		j["StartScanning"] = { {"Id", k.Id} };
	}

	void to_json(json& j, const StopScanning& k) {
		j["StopScanning"] = { {"Id", k.Id} };
	}

	void to_json(json& j, const RequestDeviceList& k) {
		j["RequestDeviceList"] = { {"Id", k.Id} };
	}

	void to_json(json& j, const StopDeviceCmd& k) {
		j["StopDeviceCmd"] = { {"Id", k.Id}, {"DeviceIndex", k.DeviceIndex} };
	}

	void to_json(json& j, const StopAllDevices& k) {
		j["StopAllDevices"] = { {"Id", k.Id} };
	}

	void to_json(json& j, const ScalarCmd& k) {
		j["ScalarCmd"] = { {"Id", k.Id}, {"DeviceIndex", k.DeviceIndex} };
		j["ScalarCmd"]["Scalars"] = json::array();
		
		for (auto& vec : k.Scalars) {
			json jTemp = { { "Index", vec.Index }, { "Scalar", vec.ScalarVal }, { "ActuatorType", vec.ActuatorType } };
			j["ScalarCmd"]["Scalars"].insert(j["ScalarCmd"]["Scalars"].end(), jTemp);
		}
	}

	void to_json(json& j, const SensorReadCmd& k) {
		j["SensorReadCmd"] = { {"Id", k.Id}, {"DeviceIndex", k.DeviceIndex}, {"SensorIndex", k.SensorIndex}, {"SensorType", k.SensorType}};
	}

	void to_json(json& j, const SensorSubscribeCmd& k) {
		j["SensorSubscribeCmd"] = { {"Id", k.Id}, {"DeviceIndex", k.DeviceIndex}, {"SensorIndex", k.SensorIndex}, {"SensorType", k.SensorType} };
	}

	void to_json(json& j, const SensorUnsubscribeCmd& k) {
		j["SensorUnsubscribeCmd"] = { {"Id", k.Id}, {"DeviceIndex", k.DeviceIndex}, {"SensorIndex", k.SensorIndex}, {"SensorType", k.SensorType} };
	}

	void from_json(const json& j, ServerInfo& k) {
		json jTemp;
		j.at("ServerInfo").get_to(jTemp);
		jTemp.at("Id").get_to(k.Id);
		jTemp.at("ServerName").get_to(k.ServerName);
		jTemp.at("MessageVersion").get_to(k.MessageVersion);
		jTemp.at("MaxPingTime").get_to(k.MaxPingTime);
	}

	void from_json(const json& j, Ok& k) {
		json jTemp;
		j.at("Ok").get_to(jTemp);
		jTemp.at("Id").get_to(k.Id);
	}

	void from_json(const json& j, Error& k) {
		json jTemp;
		j.at("Error").get_to(jTemp);
		jTemp.at("Id").get_to(k.Id);
		jTemp.at("ErrorCode").get_to(k.ErrorCode);
		jTemp.at("ErrorMessage").get_to(k.ErrorMessage);
	}

	void from_json(const json& j, DeviceRemoved& k) {
		json jTemp;
		j.at("DeviceRemoved").get_to(jTemp);
		jTemp.at("Id").get_to(k.Id);
		jTemp.at("DeviceIndex").get_to(k.DeviceIndex);
	}

	// The device conversion functions are slightly more complicated since they have some
	// nested objects and arrays, but otherwise it is simply parsing, just a lot of it.
	void from_json(const json& j, DeviceList& k) {
		json jTemp;
		j.at("DeviceList").get_to(jTemp);
		jTemp.at("Id").get_to(k.Id);

		if (jTemp["Devices"].size() > 0) {
			for (auto& el : jTemp["Devices"].items()) {
				Device tempD;
				//std::cout << el.value() << std::endl;
				auto test = el.value().contains("DeviceMessageTimingGap");
				if (el.value().contains("DeviceName")) tempD.DeviceName = el.value()["DeviceName"];

				if (el.value().contains("DeviceIndex")) tempD.DeviceIndex = el.value()["DeviceIndex"];

				if (el.value().contains("DeviceMessageTimingGap")) tempD.DeviceMessageTimingGap = el.value()["DeviceMessageTimingGap"];

				if (el.value().contains("DeviceDisplayName")) tempD.DeviceName = el.value()["DeviceDisplayName"];

				if (el.value().contains("DeviceMessages")) {
					json jTemp2;
					jTemp2 = el.value()["DeviceMessages"];

					for (auto& el2 : jTemp2.items()) {
						DeviceCmd tempCmd;
						tempCmd.CmdType = el2.key();

						if (!el2.key().compare("StopDeviceCmd")) {
							// Do something, not sure what yet
							continue;
						}

						for (auto& el3 : el2.value().items()) {
							DeviceCmdAttr tempAttr;
							//std::cout << el3.value() << std::endl;

							if (el3.value().contains("FeatureDescriptor")) tempAttr.FeatureDescriptor = el3.value()["FeatureDescriptor"];

							if (el3.value().contains("StepCount")) tempAttr.StepCount = el3.value()["StepCount"];

							if (el3.value().contains("ActuatorType")) tempAttr.ActuatorType = el3.value()["ActuatorType"];

							if (el3.value().contains("SensorType")) tempAttr.SensorType = el3.value()["SensorType"];

							if (el3.value().contains("SensorRange")) {
								//std::cout << el3.value()["SensorRange"] << std::endl;
								for (auto& el4 : el3.value()["SensorRange"].items()) {
									tempAttr.SensorRange.push_back(el4.value()[0]);
									tempAttr.SensorRange.push_back(el4.value()[1]);
								}
								//tempCmd.SensorRange.push_back(el2.value()["SensorRange"]);
							}

							//if (el2.value().contains("Endpoints")) tempCmd.Endpoints = el2.value()["Endpoints"];
							//std::cout << el2.key() << std::endl;
							//std::cout << el2.value() << std::endl;
							tempCmd.DeviceCmdAttributes.push_back(tempAttr);
						}

						tempD.DeviceMessages.push_back(tempCmd);
					}
				}
				k.Devices.push_back(tempD);
			}
		}
	}

	void from_json(const json& j, DeviceAdded& k) {
		json jTemp;
		j.at("DeviceAdded").get_to(jTemp);
		jTemp.at("Id").get_to(k.Id);

		Device tempD;

		if (jTemp.contains("DeviceName")) k.device.DeviceName = jTemp["DeviceName"];

		if (jTemp.contains("DeviceIndex")) k.device.DeviceIndex = jTemp["DeviceIndex"];

		if (jTemp.contains("DeviceMessageTimingGap")) k.device.DeviceMessageTimingGap = jTemp["DeviceMessageTimingGap"];

		if (jTemp.contains("DeviceDisplayName")) k.device.DeviceName = jTemp["DeviceDisplayName"];

		if (jTemp.contains("DeviceMessages")) {
			json jTemp2;
			jTemp2 = jTemp["DeviceMessages"];

			for (auto& el2 : jTemp2.items()) {
				DeviceCmd tempCmd;
				tempCmd.CmdType = el2.key();

				if (!el2.key().compare("StopDeviceCmd")) {
					// Do something, not sure what yet
					continue;
				}

				for (auto& el3 : el2.value().items()) {
					DeviceCmdAttr tempAttr;
					//std::cout << el3.value() << std::endl;

					if (el3.value().contains("FeatureDescriptor")) tempAttr.FeatureDescriptor = el3.value()["FeatureDescriptor"];

					if (el3.value().contains("StepCount")) tempAttr.StepCount = el3.value()["StepCount"];

					if (el3.value().contains("ActuatorType")) tempAttr.ActuatorType = el3.value()["ActuatorType"];

					if (el3.value().contains("SensorType")) tempAttr.SensorType = el3.value()["SensorType"];

					if (el3.value().contains("SensorRange")) {
						//std::cout << el3.value()["SensorRange"] << std::endl;
						for (auto& el4 : el3.value()["SensorRange"].items()) {
							tempAttr.SensorRange.push_back(el4.value()[0]);
							tempAttr.SensorRange.push_back(el4.value()[1]);
						}
					}

					//if (el2.value().contains("Endpoints")) tempCmd.Endpoints = el2.value()["Endpoints"];
					//std::cout << el2.key() << std::endl;
					//std::cout << el2.value() << std::endl;
					tempCmd.DeviceCmdAttributes.push_back(tempAttr);
				}

				k.device.DeviceMessages.push_back(tempCmd);
			}
		}
	}

	void from_json(const json& j, SensorReading& k) {
		json jTemp;
		j.at("SensorReading").get_to(jTemp);
		jTemp.at("Id").get_to(k.Id);
		jTemp.at("DeviceIndex").get_to(k.DeviceIndex);
		jTemp.at("SensorIndex").get_to(k.SensorIndex);
		jTemp.at("SensorType").get_to(k.SensorType);
		for (auto& el : jTemp["Data"].items())
			k.Data.push_back(el.value());
	}
}