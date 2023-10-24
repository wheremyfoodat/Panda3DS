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
		None,          // Cheat has been removed by the frontend or is invalid
		ActionReplay,  // CTRPF cheats
	};

	struct Cheat {
		bool enabled;
		CheatType type;
		std::vector<u32> instructions;
	};

	Cheats(Memory& mem, HIDService& hid);
	u32 addCheat(const Cheat& cheat);
	void removeCheat(u32 id);
	void enableCheat(u32 id);
	void disableCheat(u32 id);
	void reset();
	void run();

	void clear();
	bool haveCheats() const { return cheatsLoaded; }

  private:
	ActionReplay ar;  // An ActionReplay cheat machine for executing CTRPF codes
	std::vector<Cheat> cheats;
	bool cheatsLoaded = false;
};