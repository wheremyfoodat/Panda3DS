#include <iostream>
#include <fstream>

#include "crypto/aes_engine.hpp"
#include "helpers.hpp"

namespace Crypto {
	void AESEngine::loadKeys(const std::filesystem::path& path) {
		std::ifstream file(path, std::ios::in);
	
		if (file.fail()) {
			Helpers::warn("Keys: Couldn't read key file: %s", path.c_str());
			return;
		}

		while (!file.eof()) {
			std::string line;
			std::getline(file, line);

			// Skip obvious invalid lines
			if (line.empty() || line.starts_with("#")) {
				continue;
			}

			const auto parts = Helpers::split(line, '=');
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
				case 'X':
					setKeyX(slotId, key.value());
					break;
				case 'Y':
					setKeyY(slotId, key.value());
					break;
				case 'N':
					setNormalKey(slotId, key.value());
					break;
				default:
					Helpers::warn("Keys: Invalid key type %c", keyType);
					break;
			}
		}

		// As the generator key can be set at any time, force update all normal keys.
		for (std::size_t i = 0; i < AesKeySlotCount; i++) {
			updateNormalKey(i);
		}

		keysLoaded = true;
	}
};