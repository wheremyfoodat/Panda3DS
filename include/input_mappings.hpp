#pragma once

#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <toml.hpp>
#include <unordered_map>

#include "helpers.hpp"
#include "services/hid.hpp"

struct InputMappings {
	using Scancode = u32;
	using Container = std::unordered_map<Scancode, u32>;

	u32 getMapping(Scancode scancode) const {
		auto it = container.find(scancode);
		return it != container.end() ? it->second : HID::Keys::Null;
	}

	void setMapping(Scancode scancode, u32 key) { container[scancode] = key; }

	void serialize(const std::filesystem::path& path, const std::string& frontend) const {
		toml::basic_value<toml::preserve_comments, std::map> data;

		std::error_code error;
		if (std::filesystem::exists(path, error)) {
			try {
				data = toml::parse<toml::preserve_comments, std::map>(path);
			} catch (const std::exception& ex) {
				Helpers::warn("Exception trying to parse mappings file. Exception: %s\n", ex.what());
				return;
			}
		} else {
			if (error) {
				Helpers::warn("Filesystem error accessing %s (error: %s)\n", path.string().c_str(), error.message().c_str());
			}
			printf("Saving new mappings file %s\n", path.string().c_str());
		}

		data["Metadata"]["Name"] = name.empty() ? "Unnamed Mappings" : name;
		data["Metadata"]["Device"] = device.empty() ? "Unknown Device" : device;
		data["Metadata"]["Frontend"] = frontend;

		data["Mappings"] = toml::table{};

		for (const auto& [scancode, key] : container) {
			if (!data["Mappings"].contains(HID::Keys::keyToName(key))) {
				data["Mappings"][HID::Keys::keyToName(key)] = toml::array{};
			}

			data["Mappings"][HID::Keys::keyToName(key)].push_back(scancodeToName(scancode));
		}

		std::ofstream file(path, std::ios::out);
		file << data;
	}

	static std::optional<InputMappings> deserialize(const std::filesystem::path& path, const std::string& wantFrontend) {
		toml::basic_value<toml::preserve_comments, std::map> data;
		std::error_code error;
		
		if (!std::filesystem::exists(path, error)) {
			if (error) {
				Helpers::warn("Filesystem error accessing %s (error: %s)\n", path.string().c_str(), error.message().c_str());
			}
			
			return std::nullopt;
		}

		InputMappings mappings;

		try {
			data = toml::parse<toml::preserve_comments, std::map>(path);

			const auto metadata = toml::find(data, "Metadata");
			mappings.name = toml::find_or<std::string>(metadata, "Name", "Unnamed Mappings");
			mappings.device = toml::find_or<std::string>(metadata, "Device", "Unknown Device");
			
			std::string haveFrontend = toml::find_or<std::string>(metadata, "Frontend", "Unknown Frontend");

			bool equal = std::equal(haveFrontend.begin(), haveFrontend.end(), wantFrontend.begin(), wantFrontend.end(), [](char a, char b) {
				return std::tolower(a) == std::tolower(b);
			});

			if (equal) {
				Helpers::warn("Mappings file %s was created for frontend %s, but we are using frontend %s\n", path.string().c_str(), haveFrontend.c_str(), wantFrontend.c_str());
				return std::nullopt;
			}
		} catch (const std::exception& ex) {
			Helpers::warn("Exception trying to parse config file. Exception: %s\n", ex.what());
			
			return std::nullopt;
		}

		const auto& mappingsTable = toml::find_or<toml::table>(data, "Mappings", toml::table{});
		for (const auto& [key, scancodes] : mappingsTable) {
			for (const auto& scancode : scancodes.as_array()) {
				mappings.setMapping(nameToScancode(scancode.as_string()), HID::Keys::nameToKey(key));
			}
		}

		return mappings;
	}

	static InputMappings defaultKeyboardMappings();
	static std::string scancodeToName(Scancode scancode);
	static Scancode nameToScancode(const std::string& name);

  private:
	Container container;
	std::string name;
	std::string device;
};
