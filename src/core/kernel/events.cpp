#include "kernel.hpp"

const char* Kernel::resetTypeToString(u32 type) {
	switch (type) {
		case 0: return "RESET_ONESHOT";
		case 1: return "RESET_STICKY";
		case 2: return "RESET_PULSE";
		default: return "Invalid";
	}
}

Handle Kernel::makeEvent(ResetType resetType) {
	Handle ret = makeObject(KernelObjectType::Event);
	objects[ret].data = new EventData();

	auto eventData = static_cast<EventData*>(objects[ret].data);
	eventData->resetType = resetType;

	return ret;
}

// Result CreateEvent(Handle* event, ResetType resetType)
// TODO: Just like getResourceLimit this seems to output the handle in r1 even though 3dbrew doesn't mention this
// Should the handle be written both in memory and r1, or just r1?
void Kernel::createEvent() {
	const u32 outPointer = regs[0];
	const u32 resetType = regs[1];

	if (resetType > 2)
		Helpers::panic("Invalid reset type for event %d", resetType);

	printf("CreateEvent(handle pointer = %08X, resetType = %s)\n", outPointer, resetTypeToString(resetType));

	Handle handle = makeEvent(static_cast<ResetType>(resetType));
	regs[0] = SVCResult::Success;
	regs[1] = handle;
	mem.write32(outPointer, handle);
}