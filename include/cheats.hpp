#pragma once
#include <array>
#include <vector>

#include "action_replay.hpp"
#include "helpers.hpp"
#include "services/hid.hpp"

// Forward-declare this since it's just passed and we don't want to include memory.hpp and increase compile time
class Memory;

class Cheats {
  public:
	enum class CheatType {
		ActionReplay,  // CTRPF cheats
		// TODO: Other cheat devices and standards?
	};

	struct Cheat {
		CheatType type;
		std::vector<u32> instructions;
	};

	Cheats(Memory& mem, HIDService& hid);
	void addCheat(const Cheat& cheat);
	void reset();
	void run();

	void clear();
	bool haveCheats() const { return cheatsLoaded; }

  private:
	ActionReplay ar;  // An ActionReplay cheat machine for executing CTRPF codes
	std::vector<Cheat> cheats;
	bool cheatsLoaded = false;
};