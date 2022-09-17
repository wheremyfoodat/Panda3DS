#include "services/apt.hpp"

void APTService::reset() {}

void APTService::handleSyncRequest(u32 messagePointer) {
	Helpers::panic("APT service requested");
}