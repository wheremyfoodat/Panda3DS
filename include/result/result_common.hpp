#pragma once

#include <climits>
#include <cstdint>
#include <type_traits>

namespace Result {
	enum class HorizonResultLevel : uint32_t {
		Success = 0,
		Info = 1,
		Status = 25,
		Temporary = 26,
		Permanent = 27,
		Usage = 28,
		Reinitialize = 29,
		Reset = 30,
		Fatal = 31,
	};

	enum class HorizonResultSummary : uint32_t {
		Success = 0,
		NothingHappened = 1,
		WouldBlock = 2,
		OutOfResource = 3,
		NotFound = 4,
		InvalidState = 5,
		NotSupported = 6,
		InvalidArgument = 7,
		WrongArgument = 8,
		Canceled = 9,
		StatusChanged = 10,
		Internal = 11,
	};

	enum class HorizonResultModule : uint32_t {
		Common = 0,
		Kernel = 1,
		Util = 2,
		FileServer = 3,
		LoaderServer = 4,
		TCB = 5,
		OS = 6,
		DBG = 7,
		DMNT = 8,
		PDN = 9,
		GSP = 10,
		I2C = 11,
		GPIO = 12,
		DD = 13,
		CODEC = 14,
		SPI = 15,
		PXI = 16,
		FS = 17,
		DI = 18,
		HID = 19,
		CAM = 20,
		PI = 21,
		PM = 22,
		PM_LOW = 23,
		FSI = 24,
		SRV = 25,
		NDM = 26,
		NWM = 27,
		SOC = 28,
		LDR = 29,
		ACC = 30,
		RomFS = 31,
		AM = 32,
		HIO = 33,
		Updater = 34,
		MIC = 35,
		FND = 36,
		MP = 37,
		MPWL = 38,
		AC = 39,
		HTTP = 40,
		DSP = 41,
		SND = 42,
		DLP = 43,
		HIO_LOW = 44,
		CSND = 45,
		SSL = 46,
		AM_LOW = 47,
		NEX = 48,
		Friends = 49,
		RDT = 50,
		Applet = 51,
		NIM = 52,
		PTM = 53,
		MIDI = 54,
		MC = 55,
		SWC = 56,
		FatFS = 57,
		NGC = 58,
		CARD = 59,
		CARDNOR = 60,
		SDMC = 61,
		BOSS = 62,
		DBM = 63,
		Config = 64,
		PS = 65,
		CEC = 66,
		IR = 67,
		UDS = 68,
		PL = 69,
		CUP = 70,
		Gyroscope = 71,
		MCU = 72,
		NS = 73,
		News = 74,
		RO = 75,
		GD = 76,
		CardSPI = 77,
		EC = 78,
		WebBrowser = 79,
		Test = 80,
		ENC = 81,
		PIA = 82,
		ACT = 83,
		VCTL = 84,
		OLV = 85,
		NEIA = 86,
		NPNS = 87,
		AVD = 90,
		L2B = 91,
		MVD = 92,
		NFC = 93,
		UART = 94,
		SPM = 95,
		QTM = 96,
		NFP = 97,
	};

	class HorizonResult {
	  private:
		static const uint32_t DescriptionBits = 10;
		static const uint32_t ModuleBits = 8;
		static const uint32_t ReservedBits = 3;
		static const uint32_t SummaryBits = 6;
		static const uint32_t LevelBits = 5;

		static const uint32_t DescriptionOffset = 0;
		static const uint32_t ModuleOffset = DescriptionOffset + DescriptionBits;
		static const uint32_t SummaryOffset = ModuleOffset + ModuleBits + ReservedBits;
		static const uint32_t LevelOffset = SummaryOffset + SummaryBits;

		static_assert(DescriptionBits + ModuleBits + SummaryBits + LevelBits + ReservedBits == sizeof(uint32_t) * CHAR_BIT, "Invalid Result size");

		uint32_t m_value;

		constexpr inline uint32_t getBitsValue(int offset, int amount) {
			return (m_value >> offset) & ~(~0 << amount);
		}

		static constexpr inline uint32_t makeValue(uint32_t description, uint32_t module, uint32_t summary, uint32_t level) {
			return (description << DescriptionOffset) | (module << ModuleOffset) | (summary << SummaryOffset) | (level << LevelOffset);
		}

	  public:
		constexpr HorizonResult() : m_value(0) {}
		constexpr HorizonResult(uint32_t value) : m_value(value) {}
		constexpr HorizonResult(uint32_t description, HorizonResultModule module, HorizonResultSummary summary, HorizonResultLevel level) : m_value(makeValue(description, static_cast<uint32_t>(module), static_cast<uint32_t>(summary), static_cast<uint32_t>(level))) {}
		constexpr operator uint32_t() const { return m_value; }

		constexpr inline uint32_t getDescription() {
			return getBitsValue(DescriptionOffset, DescriptionBits);
		}

		constexpr inline HorizonResultModule getModule() {
			return static_cast<HorizonResultModule>(getBitsValue(ModuleOffset, ModuleBits));
		}

		constexpr inline HorizonResultSummary getSummary() {
			return static_cast<HorizonResultSummary>(getBitsValue(SummaryOffset, SummaryBits));
		}

		constexpr inline HorizonResultLevel getLevel() {
			return static_cast<HorizonResultLevel>(getBitsValue(LevelOffset, LevelBits));
		}

		constexpr inline uint32_t getRawValue() {
			return m_value;
		}

		constexpr inline bool isSuccess() {
			return m_value == 0;
		}

		constexpr inline bool isFailure() {
			return m_value != 0;
		}
	};

	static_assert(std::is_trivially_destructible<HorizonResult>::value, "std::is_trivially_destructible<HorizonResult>::value");

#define DEFINE_HORIZON_RESULT_MODULE(ns, value)                                     \
	namespace ns::Detail {                                                          \
		static constexpr HorizonResultModule ModuleId = HorizonResultModule::value; \
	}

#define DEFINE_HORIZON_RESULT(name, description, summary, level) \
	static constexpr HorizonResult name(description, Detail::ModuleId, HorizonResultSummary::summary, HorizonResultLevel::level);

	static constexpr HorizonResult Success(0);
	static constexpr HorizonResult FailurePlaceholder(UINT32_MAX);
};  // namespace Result
