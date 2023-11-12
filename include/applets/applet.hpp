#pragma once

#include "helpers.hpp"
#include "memory.hpp"
#include "result/result.hpp"

namespace Applets {
	namespace AppletIDs {
		enum : u32 {
			None = 0,
			SysAppletMask = 0x100,
			HomeMenu = 0x101,
			AltMenu = 0x103,
			Camera = 0x110,
			Friends = 0x112,
			GameNotes = 0x113,
			Browser = 0x114,
			InstructionManual = 0x115,
			Notifications = 0x116,
			Miiverse = 0x117,
			MiiversePosting = 0x118,
			AmiiboSettings = 0x119,
			SysLibraryAppletMask = 0x200,
			SoftwareKeyboard = 0x201,
			MiiSelector = 0x202,
			PNote = 0x204,  // TODO: What dis?
			SNote = 0x205,  // What is this too?
			ErrDisp = 0x206,
			EshopMint = 0x207,
			CirclePadProCalib = 0x208,
			Notepad = 0x209,
			Application = 0x300,
			EshopTiger = 0x301,
			LibraryAppletMask = 0x400,
			SoftwareKeyboard2 = 0x401,
			MiiSelector2 = 0x402,
			Pnote2 = 0x404,
			SNote2 = 0x405,
			ErrDisp2 = 0x406,
			EshopMint2 = 0x407,
			CirclePadProCalib2 = 0x408,
			Notepad2 = 0x409,
		};
	}

	enum class APTSignal : u32 {
		None = 0x0,
		Wakeup = 0x1,
		Request = 0x2,
		Response = 0x3,
		Exit = 0x4,
		Message = 0x5,
		HomeButtonSingle = 0x6,
		HomeButtonDouble = 0x7,
		DspSleep = 0x8,
		DspWakeup = 0x9,
		WakeupByExit = 0xA,
		WakeupByPause = 0xB,
		WakeupByCancel = 0xC,
		WakeupByCancelAll = 0xD,
		WakeupByPowerButtonClick = 0xE,
		WakeupToJumpHome = 0xF,
		RequestForSysApplet = 0x10,
		WakeupToLaunchApplication = 0x11,
	};

	struct Parameter {
		u32 senderID;
		u32 destID;
		u32 signal;
		std::vector<u8> data;
	};

	class AppletBase {
	  protected:
		Memory& mem;
		std::optional<Parameter>& nextParameter;

	  public:
		virtual const char* name() = 0;

		// Called by APT::StartLibraryApplet and similar
		virtual Result::HorizonResult start() = 0;
		// Transfer parameters from application -> applet
		virtual Result::HorizonResult receiveParameter(const Parameter& parameter) = 0;
		virtual void reset() = 0;

		AppletBase(Memory& mem, std::optional<Parameter>& nextParam) : mem(mem), nextParameter(nextParam) {}
	};
}  // namespace Applets