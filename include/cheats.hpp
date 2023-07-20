#pragma once
#include <array>
#include <vector>

#include "action_replay.hpp"
#include "helpers.hpp"

// Forward-declare this since it's just passed and we don't want to include memory.hpp and increase compile time
class Memory;

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

	Cheats(Memory& mem);
	void addCheat(const Cheat& cheat);
	void reset();
	void run();

  private:
	ActionReplay ar; // An ActionReplay cheat machine for executing CTRPF codes
	std::vector<Cheat> cheats;
};