#include <string>

#include "applets/applet.hpp"
#include "swap.hpp"

namespace Applets {
	struct MiiConfig {
		u8 enableCancelButton;
		u8 enableGuestMii;
		u8 showOnTopScreen;
		std::array<u8, 0x5> pad1;
		std::array<u16_le, 0x40> title;
		std::array<u8, 0x4> pad2;
		u8 showGuestMiis;
		std::array<u8, 0x3> pad3;
		u32 initiallySelectedIndex;
		std::array<u8, 0x6> guestMiiWhitelist;
		std::array<u8, 0x64> userMiiWhitelist;
		std::array<u8, 0x2> pad4;
		u32 magicValue;
	};
	static_assert(sizeof(MiiConfig) == 0x104, "Mii config size is wrong");

	// Some members of this struct are not properly aligned so we need pragma pack
#pragma pack(push, 1)
	struct MiiData {
		u8 version;
		u8 miiOptions;
		u8 miiPos;
		u8 consoleID;

		u64_be systemID;
		u32_be miiID;
		std::array<u8, 0x6> creatorMAC;
		u16 padding;

		u16_be miiDetails;
		std::array<char16_t, 0xA> miiName;
		u8 height;
		u8 width;

		u8 faceStyle;
		u8 faceDetails;
		u8 hairStyle;
		u8 hairDetails;
		u32_be eyeDetails;
		u32_be eyebrowDetails;
		u16_be noseDetails;
		u16_be mouthDetails;
		u16_be moustacheDetails;
		u16_be beardDetails;
		u16_be glassesDetails;
		u16_be moleDetails;

		std::array<char16_t, 0xA> authorName;
	};
#pragma pack(pop)
	static_assert(sizeof(MiiData) == 0x5C, "MiiData structure has incorrect size");

	struct MiiResult {
		u32_be returnCode;
		u32_be isGuestMiiSelected;
		u32_be selectedGuestMiiIndex;
		MiiData selectedMiiData;
		u16_be unknown1;
		u16_be miiChecksum;
		std::array<u16_le, 0xC> guestMiiName;
	};
	static_assert(sizeof(MiiResult) == 0x84, "MiiResult structure has incorrect size");

	class MiiSelectorApplet final : public AppletBase {
	  public:
		virtual const char* name() override { return "Mii Selector"; }
		virtual Result::HorizonResult start(const MemoryBlock* sharedMem, const std::vector<u8>& parameters, u32 appID) override;
		virtual Result::HorizonResult receiveParameter(const Applets::Parameter& parameter) override;
		virtual void reset() override;

		MiiResult output;
		MiiConfig config;
		MiiResult getDefaultMii();
		MiiSelectorApplet(Memory& memory, std::optional<Parameter>& nextParam) : AppletBase(memory, nextParam) {}
	};
}  // namespace Applets