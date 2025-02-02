#pragma once
#include <boost/container/flat_map.hpp>
#include <boost/container/static_vector.hpp>
#include <limits>

#include "helpers.hpp"
#include "logger.hpp"

struct Scheduler {
	enum class EventType {
		VBlank = 0,          // End of frame event
		UpdateTimers = 1,    // Update kernel timer objects
		RunDSP = 2,          // Make the emulated DSP run for one audio frame
		ThreadWakeup = 3,    // A thread is going to wake up and we need to reschedule threads
		SignalY2R = 4,       // Signal that a Y2R conversion has finished
		Panic = 4,           // Dummy event that is always pending and should never be triggered (Timestamp = UINT64_MAX)
		TotalNumberOfEvents  // How many event types do we have in total?
	};
	static constexpr usize totalNumberOfEvents = static_cast<usize>(EventType::TotalNumberOfEvents);
	static constexpr u64 arm11Clock = 268111856;

	template <typename Key, typename Val, usize size>
	using EventMap = boost::container::flat_multimap<Key, Val, std::less<Key>, boost::container::static_vector<std::pair<Key, Val>, size>>;

	EventMap<u64, EventType, totalNumberOfEvents> events;
	u64 currentTimestamp = 0;
	u64 nextTimestamp = 0;

	// Set nextTimestamp to the timestamp of the next event
	void updateNextTimestamp() { nextTimestamp = events.cbegin()->first; }

	void addEvent(EventType type, u64 timestamp) {
		events.emplace(timestamp, type);
		updateNextTimestamp();
	}

	void removeEvent(EventType type) {
		for (auto it = events.begin(); it != events.end(); it++) {
			// Find first event of type "type" and remove it.
			// Our scheduler shouldn't have duplicate events, so it's safe to exit when an event is found
			if (it->second == type) {
				events.erase(it);
				updateNextTimestamp();
				break;
			}
		}
	};

	void reset() {
		currentTimestamp = 0;

		// Clear any pending events
		events.clear();
		addEvent(Scheduler::EventType::VBlank, arm11Clock / 60);

		// Add a dummy event to always keep the scheduler non-empty
		addEvent(EventType::Panic, std::numeric_limits<u64>::max());
	}

  private:
	static constexpr u64 MAX_VALUE_TO_MULTIPLY = std::numeric_limits<s64>::max() / arm11Clock;

  public:
	// Function for converting time units to cycles for various kernel functions
	// Thank you Citra
	static constexpr s64 nsToCycles(float ns) { return s64(arm11Clock * (0.000000001f) * ns); }
	static constexpr s64 nsToCycles(int ns) { return arm11Clock * s64(ns) / 1000000000; }

	static constexpr s64 nsToCycles(s64 ns) {
		if (ns / 1000000000 > static_cast<s64>(MAX_VALUE_TO_MULTIPLY)) {
			return std::numeric_limits<s64>::max();
		}

		if (ns > static_cast<s64>(MAX_VALUE_TO_MULTIPLY)) {
			return arm11Clock * (ns / 1000000000);
		}

		return (arm11Clock * ns) / 1000000000;
	}

	static constexpr s64 nsToCycles(u64 ns) {
		if (ns / 1000000000 > MAX_VALUE_TO_MULTIPLY) {
			return std::numeric_limits<s64>::max();
		}

		if (ns > MAX_VALUE_TO_MULTIPLY) {
			return arm11Clock * (s64(ns) / 1000000000);
		}

		return (arm11Clock * s64(ns)) / 1000000000;
	}
};