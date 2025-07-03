#pragma once
#include <functional>
#include <string>

#include "helpers.hpp"

// We allow Lua scripts to intercept service calls and allow their own code to be ran on SyncRequests
// For example, if we want to intercept dsp::DSP ReadPipe (Header: 0x000E00C0), the "serviceName" field would be "dsp::DSP"
// and the "function" field would be 0x000E00C0
struct InterceptedService {
	std::string serviceName;  // Name of the service whose function
	u32 function;             // Header of the function to intercept

	InterceptedService(const std::string& name, u32 header) : serviceName(name), function(header) {}
	bool operator==(const InterceptedService& other) const { return serviceName == other.serviceName && function == other.function; }
};

// Define hashing function for InterceptedService to use it with unordered_map
namespace std {
	template <>
	struct hash<InterceptedService> {
		usize operator()(const InterceptedService& s) const noexcept {
			const usize hash1 = std::hash<std::string>{}(s.serviceName);
			const usize hash2 = std::hash<u32>{}(s.function);
			return hash1 ^ (hash2 << 1);
		}
	};
}  // namespace std
