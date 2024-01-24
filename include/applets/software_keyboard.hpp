#include <array>

#include "applets/applet.hpp"
#include "swap.hpp"

namespace Applets {
	// Software keyboard definitions adapted from libctru/Citra
	// Keyboard input filtering flags. Allows the caller to specify what input is explicitly not allowed
	namespace SoftwareKeyboardFilter {
		enum Filter : u32 {
			Digits = 1,          // Disallow the use of more than a certain number of digits (0 or more)
			At = 1 << 1,         // Disallow the use of the @ sign.
			Percent = 1 << 2,    // Disallow the use of the % sign.
			Backslash = 1 << 3,  // Disallow the use of the \ sign.
			Profanity = 1 << 4,  // Disallow profanity using Nintendo's profanity filter.
			Callback = 1 << 5,   // Use a callback in order to check the input.
		};
	}  // namespace SoftwareKeyboardFilter

	// Keyboard features.
	namespace SoftwareKeyboardFeature {
		enum Feature {
			Parental = 1,              // Parental PIN mode.
			DarkenTopScreen = 1 << 1,  // Darken the top screen when the keyboard is shown.
			PredictiveInput = 1 << 2,  // Enable predictive input (necessary for Kanji input in JPN systems).
			Multiline = 1 << 3,        // Enable multiline input.
			FixedWidth = 1 << 4,       // Enable fixed-width mode.
			AllowHome = 1 << 5,        // Allow the usage of the HOME button.
			AllowReset = 1 << 6,       // Allow the usage of a software-reset combination.
			AllowPower = 1 << 7,       // Allow the usage of the POWER button.
			DefaultQWERTY = 1 << 9,    // Default to the QWERTY page when the keyboard is shown.
		};
	}  // namespace SoftwareKeyboardFeature

	class SoftwareKeyboardApplet final : public AppletBase {
	  public:
		static constexpr int MAX_BUTTON = 3;              // Maximum number of buttons that can be in the keyboard.
		static constexpr int MAX_BUTTON_TEXT_LEN = 16;    // Maximum button text length, in UTF-16 code units.
		static constexpr int MAX_HINT_TEXT_LEN = 64;      // Maximum hint text length, in UTF-16 code units.
		static constexpr int MAX_CALLBACK_MSG_LEN = 256;  // Maximum filter callback error message length, in UTF-16 code units.

		// Keyboard types
		enum class SoftwareKeyboardType : u32 {
			Normal,   // Normal keyboard with several pages (QWERTY/accents/symbol/mobile)
			QWERTY,   // QWERTY keyboard only.
			NumPad,   // Number pad.
			Western,  // On JPN systems, a text keyboard without Japanese input capabilities, otherwise same as SWKBD_TYPE_NORMAL.
		};

		// Keyboard dialog buttons.
		enum class SoftwareKeyboardButtonConfig : u32 {
			SingleButton,  // Ok button
			DualButton,    // Cancel | Ok buttons
			TripleButton,  // Cancel | I Forgot | Ok buttons
			NoButton,      // No button (returned by swkbdInputText in special cases)
		};

		// Accepted input types.
		enum class SoftwareKeyboardValidInput : u32 {
			Anything,          // All inputs are accepted.
			NotEmpty,          // Empty inputs are not accepted.
			NotEmptyNotBlank,  // Empty or blank inputs (consisting solely of whitespace) are not accepted.
			NotBlank,          // Blank inputs (consisting solely of whitespace) are not accepted, but empty inputs are.
			FixedLen,          // The input must have a fixed length (specified by maxTextLength in swkbdInit)
		};

		// Keyboard password modes.
		enum class SoftwareKeyboardPasswordMode : u32 {
			None,       // Characters are not concealed.
			Hide,       // Characters are concealed immediately.
			HideDelay,  // Characters are concealed a second after they've been typed.
		};

		// Keyboard filter callback return values.
		enum class SoftwareKeyboardCallbackResult : u32 {
			OK,        // Specifies that the input is valid.
			Close,     // Displays an error message, then closes the keyboard.
			Continue,  // Displays an error message and continues displaying the keyboard.
		};

		// Keyboard return values.
		enum class SoftwareKeyboardResult : s32 {
			None = -1,          // Dummy/unused.
			InvalidInput = -2,  // Invalid parameters to swkbd.
			OutOfMem = -3,      // Out of memory.

			D0Click = 0,  // The button was clicked in 1-button dialogs.
			D1Click0,     // The left button was clicked in 2-button dialogs.
			D1Click1,     // The right button was clicked in 2-button dialogs.
			D2Click0,     // The left button was clicked in 3-button dialogs.
			D2Click1,     // The middle button was clicked in 3-button dialogs.
			D2Click2,     // The right button was clicked in 3-button dialogs.

			HomePressed = 10,  // The HOME button was pressed.
			ResetPressed,      // The soft-reset key combination was pressed.
			PowerPressed,      // The POWER button was pressed.

			ParentalOK = 20,  // The parental PIN was verified successfully.
			ParentalFail,     // The parental PIN was incorrect.

			BannedInput = 30,  // The filter callback returned SoftwareKeyboardCallback::CLOSE.
		};

		struct SoftwareKeyboardConfig {
			enum_le<SoftwareKeyboardType> type;
			enum_le<SoftwareKeyboardButtonConfig> numButtonsM1;
			enum_le<SoftwareKeyboardValidInput> validInput;
			enum_le<SoftwareKeyboardPasswordMode> passwordMode;
			s32_le isParentalScreen;
			s32_le darkenTopScreen;
			u32_le filterFlags;
			u32_le saveStateFlags;
			u16_le maxTextLength;
			u16_le dictWordCount;
			u16_le maxDigits;
			std::array<std::array<u16_le, MAX_BUTTON_TEXT_LEN + 1>, MAX_BUTTON> buttonText;
			std::array<u16_le, 2> numpadKeys;
			std::array<u16_le, MAX_HINT_TEXT_LEN + 1> hintText;  // Text to display when asking the user for input
			bool predictiveInput;
			bool multiline;
			bool fixedWidth;
			bool allowHome;
			bool allowReset;
			bool allowPower;
			bool unknown;
			bool defaultQwerty;
			std::array<bool, 4> buttonSubmitsText;
			u16_le language;

			u32_le initialTextOffset;  // Offset of the default text in the output SharedMemory
			u32_le dictOffset;
			u32_le initialStatusOffset;
			u32_le initialLearningOffset;
			u32_le sharedMemorySize;  // Size of the SharedMemory
			u32_le version;

			enum_le<SoftwareKeyboardResult> returnCode;

			u32_le statusOffset;
			u32_le learningOffset;

			u32_le textOffset;  // Offset in the SharedMemory where the output text starts
			u16_le textLength;  // Length in characters of the output text

			enum_le<SoftwareKeyboardCallbackResult> callbackResult;
			std::array<u16_le, MAX_CALLBACK_MSG_LEN + 1> callbackMessage;
			bool skipAtCheck;
			std::array<u8, 0xAB> pad;
		};
		static_assert(sizeof(SoftwareKeyboardConfig) == 0x400, "Software keyboard config size is wrong");

		virtual const char* name() override { return "Software Keyboard"; }
		virtual Result::HorizonResult start(const MemoryBlock& sharedMem, const std::vector<u8>& parameters, u32 appID) override;
		virtual Result::HorizonResult receiveParameter(const Applets::Parameter& parameter) override;
		virtual void reset() override;

		SoftwareKeyboardApplet(Memory& memory, std::optional<Parameter>& nextParam) : AppletBase(memory, nextParam) {}
		void closeKeyboard(u32 appID);

		SoftwareKeyboardConfig config;
	};
}  // namespace Applets