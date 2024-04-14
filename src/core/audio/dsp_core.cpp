#include "audio/dsp_core.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_map>

#include "audio/hle_core.hpp"
#include "audio/null_core.hpp"
#include "audio/teakra_core.hpp"

std::unique_ptr<Audio::DSPCore> Audio::makeDSPCore(DSPCore::Type type, Memory& mem, Scheduler& scheduler, DSPService& dspService) {
	std::unique_ptr<DSPCore> core;

	switch (type) {
		case DSPCore::Type::Null: core = std::make_unique<NullDSP>(mem, scheduler, dspService); break;
		case DSPCore::Type::Teakra: core = std::make_unique<TeakraDSP>(mem, scheduler, dspService); break;
		case DSPCore::Type::HLE: core = std::make_unique<HLE_DSP>(mem, scheduler, dspService); break;

		default:
			Helpers::warn("Invalid DSP core selected!");
			core = std::make_unique<NullDSP>(mem, scheduler, dspService);
			break;
	}

	mem.setDSPMem(core->getDspMemory());
	return core;
}

Audio::DSPCore::Type Audio::DSPCore::typeFromString(std::string inString) {
	// Transform to lower-case to make the setting case-insensitive
	std::transform(inString.begin(), inString.end(), inString.begin(), [](unsigned char c) { return std::tolower(c); });

	static const std::unordered_map<std::string, Audio::DSPCore::Type> map = {
		{"null", Audio::DSPCore::Type::Null},     {"none", Audio::DSPCore::Type::Null}, {"lle", Audio::DSPCore::Type::Teakra},
		{"teakra", Audio::DSPCore::Type::Teakra}, {"hle", Audio::DSPCore::Type::HLE},   {"fast", Audio::DSPCore::Type::HLE},
	};

	if (auto search = map.find(inString); search != map.end()) {
		return search->second;
	}

	printf("Invalid DSP type. Defaulting to null\n");
	return Audio::DSPCore::Type::Null;
}

const char* Audio::DSPCore::typeToString(Audio::DSPCore::Type type) {
	switch (type) {
		case Audio::DSPCore::Type::Null: return "null";
		case Audio::DSPCore::Type::Teakra: return "teakra";
		case Audio::DSPCore::Type::HLE: return "hle";
		default: return "invalid";
	}
}
