#pragma once
#include <boost/container/flat_map.hpp>
#include <boost/container/static_vector.hpp>
#include <limits>
#include <ranges>

#include "helpers.hpp"

struct Scheduler {
	enum class EventType {
		VBlank = 0,          // End of frame event
		Panic = 1,           // Dummy event that is always pending and should never be triggered (Timestamp = UINT64_MAX)
		TotalNumberOfEvents  // How many event types do we have in total?
	};
	static constexpr usize totalNumberOfEvents = static_cast<usize>(EventType::TotalNumberOfEvents);

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
		auto it = std::ranges::find_if(events, [type](decltype(events)::const_reference pair) { return pair.second == type; });

		if (it != events.end()) {
			events.erase(it);
			updateNextTimestamp();
		}
	};

	void reset() {
		currentTimestamp = 0;

		// Clear any pending events
		events.clear();
		// Add a dummy event to always keep the scheduler non-empty
		addEvent(EventType::Panic, std::numeric_limits<u64>::max());
	}
};