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

	class AppletBase {
		Memory& mem;

	  public:
		// Called by APT::StartLibraryApplet and similar
		virtual Result::HorizonResult start() = 0;
		virtual void reset() = 0;

		AppletBase(Memory& mem) : mem(mem) {}
	};
}  // namespace Applets