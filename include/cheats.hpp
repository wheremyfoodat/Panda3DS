#pragma once
#include <array>
#include <vector>

#include "action_replay.hpp"
#include "helpers.hpp"

class Cheats {
  public:
	enum class CheatType {
		ActionReplay,  // CTRPF cheats
		Gateway,
	};

	struct Cheat {
		CheatType type;
		std::vector<u32> instructions;
	};

	Cheats();
	void addCheat(const Cheat& cheat);
	void runCheats();
	void reset();

  private:
	ActionReplay ar; // An ActionReplay cheat machine for executing CTRPF codes
	std::vector<Cheat> cheats;
};