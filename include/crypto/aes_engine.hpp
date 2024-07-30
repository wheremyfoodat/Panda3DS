#pragma once

#include <array>
#include <climits>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <optional>
#include <vector>

#include "helpers.hpp"
#include "io_file.hpp"
#include "swap.hpp"

namespace Crypto {
	constexpr usize AesKeySize = 0x10;
	using AESKey = std::array<u8, AesKeySize>;

	struct Seed {
		u64_le titleID;
		AESKey seed;
		std::array<u8, 8> pad;
	};

	template <usize N>
	static std::array<u8, N> rolArray(const std::array<u8, N>& value, usize bits) {
		const auto bitWidth = N * CHAR_BIT;

		bits %= bitWidth;

		const auto byteShift = bits / CHAR_BIT;
		const auto bitShift = bits % CHAR_BIT;

		std::array<u8, N> result;

		for (usize i = 0; i < N; i++) {
			result[i] = ((value[(i + byteShift) % N] << bitShift) | (value[(i + byteShift + 1) % N] >> (CHAR_BIT - bitShift))) & UINT8_MAX;
		}

		return result;
	}

	template <usize N>
	static std::array<u8, N> addArray(const std::array<u8, N>& a, const std::array<u8, N>& b) {
		std::array<u8, N> result;
		usize sum = 0;
		usize carry = 0;

		for (std::int64_t i = N - 1; i >= 0; i--) {
			sum = a[i] + b[i] + carry;
			carry = sum >> CHAR_BIT;
			result[i] = static_cast<u8>(sum & UINT8_MAX);
		}

		return result;
	}

	template <usize N>
	static std::array<u8, N> xorArray(const std::array<u8, N>& a, const std::array<u8, N>& b) {
		std::array<u8, N> result;

		for (usize i = 0; i < N; i++) {
			result[i] = a[i] ^ b[i];
		}

		return result;
	}

	static std::optional<AESKey> createKeyFromHex(const std::string& hex) {
		if (hex.size() < 32) {
			return {};
		}

		AESKey rawKey;
		for (usize i = 0; i < rawKey.size(); i++) {
			rawKey[i] = static_cast<u8>(std::stoi(hex.substr(i * 2, 2), 0, 16));
		}

		return rawKey;
	}

	struct AESKeySlot {
		std::optional<AESKey> keyX = std::nullopt;
		std::optional<AESKey> keyY = std::nullopt;
		std::optional<AESKey> normalKey = std::nullopt;
	};

	enum KeySlotId : usize {
		NCCHKey0 = 0x2C,
		NCCHKey1 = 0x25,
		NCCHKey2 = 0x18,
		NCCHKey3 = 0x1B,
	};

	class AESEngine {
	  private:
		constexpr static usize AesKeySlotCount = 0x40;

		std::optional<AESKey> m_generator = std::nullopt;
		std::array<AESKeySlot, AesKeySlotCount> m_slots;
		bool keysLoaded = false;

		std::vector<Seed> seeds;
		IOFile seedDatabase;
		bool seedsLoaded = false;

		constexpr void updateNormalKey(usize slotId) {
			if (m_generator.has_value() && hasKeyX(slotId) && hasKeyY(slotId)) {
				auto& keySlot = m_slots.at(slotId);
				AESKey keyX = keySlot.keyX.value();
				AESKey keyY = keySlot.keyY.value();

				keySlot.normalKey = rolArray(addArray(xorArray(rolArray(keyX, 2), keyY), m_generator.value()), 87);
			}
		}

	  public:
		AESEngine() {}
		void loadKeys(const std::filesystem::path& path);
		void setSeedPath(const std::filesystem::path& path);
		// Returns true on success, false on failure
		bool loadSeeds();

		bool haveKeys() { return keysLoaded; }
		bool haveGenerator() { return m_generator.has_value(); }

		constexpr bool hasKeyX(usize slotId) {
			if (slotId >= AesKeySlotCount) {
				return false;
			}

			return m_slots.at(slotId).keyX.has_value();
		}

		constexpr AESKey getKeyX(usize slotId) { return m_slots.at(slotId).keyX.value_or(AESKey{}); }

		constexpr void setKeyX(usize slotId, const AESKey& key) {
			if (slotId < AesKeySlotCount) {
				m_slots.at(slotId).keyX = key;
				updateNormalKey(slotId);
			}
		}

		constexpr bool hasKeyY(usize slotId) {
			if (slotId >= AesKeySlotCount) {
				return false;
			}

			return m_slots.at(slotId).keyY.has_value();
		}

		constexpr AESKey getKeyY(usize slotId) { return m_slots.at(slotId).keyY.value_or(AESKey{}); }

		constexpr void setKeyY(usize slotId, const AESKey& key) {
			if (slotId < AesKeySlotCount) {
				m_slots.at(slotId).keyY = key;
				updateNormalKey(slotId);
			}
		}

		constexpr bool hasNormalKey(usize slotId) {
			if (slotId >= AesKeySlotCount) {
				return false;
			}

			return m_slots.at(slotId).normalKey.has_value();
		}

		constexpr AESKey getNormalKey(usize slotId) { return m_slots.at(slotId).normalKey.value_or(AESKey{}); }

		constexpr void setNormalKey(usize slotId, const AESKey& key) {
			if (slotId < AesKeySlotCount) {
				m_slots.at(slotId).normalKey = key;
			}
		}

		std::optional<AESKey> getSeedFromDB(u64 titleID);
	};
}  // namespace Crypto