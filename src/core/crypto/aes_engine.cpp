#include "crypto/aes_engine.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "helpers.hpp"

namespace Crypto {
	void AESEngine::loadKeys(const std::filesystem::path& path) {
		std::ifstream file(path, std::ios::in);

		if (file.fail()) {
			Helpers::warn("Keys: Couldn't read key file: %s", path.c_str());
			return;
		}

		auto splitString = [](const std::string& s, const char c) -> std::vector<std::string> {
			std::istringstream tmp(s);
			std::vector<std::string> result(1);

			while (std::getline(tmp, *result.rbegin(), c)) {
				result.emplace_back();
			}

			// Remove temporary slot
			result.pop_back();
			return result;
		};

		while (!file.eof()) {
			std::string line;
			std::getline(file, line);

			// Skip obvious invalid lines
			if (line.empty() || line.starts_with("#")) {
				continue;
			}

			const auto parts = splitString(line, '=');
			if (parts.size() != 2) {
				Helpers::warn("Keys: Failed to parse %s", line.c_str());
				continue;
			}

			const std::string& name = parts[0];
			const std::string& rawKeyHex = parts[1];

			std::size_t slotId;
			char keyType;

			bool is_generator = name == "generator";
			if (!is_generator && std::sscanf(name.c_str(), "slot0x%zXKey%c", &slotId, &keyType) != 2) {
				Helpers::warn("Keys: Ignoring unknown key %s", name.c_str());
				continue;
			}

			auto key = createKeyFromHex(rawKeyHex);

			if (!key.has_value()) {
				Helpers::warn("Keys: Failed to parse raw key %s", rawKeyHex.c_str());
				continue;
			}

			if (is_generator) {
				m_generator = key;
				continue;
			}

			if (slotId >= AesKeySlotCount) {
				Helpers::warn("Keys: Invalid key slot id %u", slotId);
				continue;
			}

			switch (keyType) {
				case 'X': setKeyX(slotId, key.value()); break;
				case 'Y': setKeyY(slotId, key.value()); break;
				case 'N': setNormalKey(slotId, key.value()); break;
				default: Helpers::warn("Keys: Invalid key type %c", keyType); break;
			}
		}

		// As the generator key can be set at any time, force update all normal keys.
		for (std::size_t i = 0; i < AesKeySlotCount; i++) {
			updateNormalKey(i);
		}

		keysLoaded = true;
	}

	void AESEngine::setSeedPath(const std::filesystem::path& path) { seedDatabase.open(path, "rb"); }

	// Loads seeds from a seed file, return true on success and false on failure
	bool AESEngine::loadSeeds() {
		if (!seedDatabase.isOpen()) {
			return false;
		}

		// The # of seeds is stored at offset 0
		u32_le seedCount = 0;

		if (!seedDatabase.rewind()) {
			return false;
		}

		auto [success, size] = seedDatabase.readBytes(&seedCount, sizeof(u32));
		if (!success || size != sizeof(u32)) {
			return false;
		}

		// Key data starts from offset 16
		if (!seedDatabase.seek(16)) {
			return false;
		}

		Crypto::Seed seed;
		for (uint i = 0; i < seedCount; i++) {
			std::tie(success, size) = seedDatabase.readBytes(&seed, sizeof(seed));
			if (!success || size != sizeof(seed)) {
				return false;
			}

			seeds.push_back(seed);
		}

		return true;
	}

	std::optional<Crypto::AESKey> AESEngine::getSeedFromDB(u64 titleID) {
		// We don't have a seed db nor any seeds loaded, return nullopt
		if (!seedDatabase.isOpen() && seeds.empty()) {
			return std::nullopt;
		}

		// We have a seed DB but haven't loaded the seeds yet, so load them
		if (seedDatabase.isOpen() && seeds.empty()) {
			bool success = loadSeeds();
			if (!success) {
				return std::nullopt;
			}
		}

		for (Crypto::Seed& seed : seeds) {
			if (seed.titleID == titleID) {
				return seed.seed;
			}
		}

		return std::nullopt;
	}
};  // namespace Crypto