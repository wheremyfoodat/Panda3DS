#pragma once
#include <string>
#include <type_traits>
#include <utility>

#include "handles.hpp"

// Helpers for constructing std::maps to look up OS services.
// We want to be able to map both service names -> services (Used for OS emulation)
// And service handles -> services (For Lua service call intercepts)
using ServiceMapEntry = std::pair<std::string, HorizonHandle>;

// Comparator for constructing a name->handle service map
struct ServiceMapByNameComparator {
	// The comparators must be transparent, as our search key is different from our set key
	// Our set key is a ServiceMapEntry, while the comparator each time is either the name or the service handle
	using is_transparent = std::true_type;
	bool operator()(const ServiceMapEntry& lhs, std::string_view rhs) const { return lhs.first < rhs; }
	bool operator()(std::string_view lhs, const ServiceMapEntry& rhs) const { return lhs < rhs.first; }
	bool operator()(const ServiceMapEntry& lhs, const ServiceMapEntry& rhs) const { return lhs.first < rhs.first; }
};

// Comparator for constructing a handle->name service map
struct ServiceMapByHandleComparator {
	using is_transparent = std::true_type;
	bool operator()(const ServiceMapEntry& lhs, HorizonHandle rhs) const { return lhs.second < rhs; }
	bool operator()(HorizonHandle lhs, const ServiceMapEntry& rhs) const { return lhs < rhs.second; }
	bool operator()(const ServiceMapEntry& lhs, const ServiceMapEntry& rhs) const { return lhs.second < rhs.second; }
};
