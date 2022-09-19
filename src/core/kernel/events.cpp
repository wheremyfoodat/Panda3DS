#include "kernel.hpp"

const char* Kernel::resetTypeToString(u32 type) {
	switch (type) {
		case 0: return "One shot";
		case 1: return "Sticky";
		case 2: return "Pulse";
		default: return "Invalid";
	}
}

Handle Kernel::makeEvent(ResetType resetType) {
	Handle ret = makeObject(KernelObjectType::Event);
	objects[ret].data = new Event(resetType);
	return ret;
}

// Result CreateEvent(Handle* event, ResetType resetType)
void Kernel::createEvent() {
	const u32 outPointer = regs[0];
	const u32 resetType = regs[1];

	if (resetType > 2)
		Helpers::panic("Invalid reset type for event %d", resetType);

	printf("CreateEvent(handle pointer = %08X, resetType = %s)\n", outPointer, resetTypeToString(resetType));

	regs[0] = SVCResult::Success;
	regs[1] = makeEvent(static_cast<ResetType>(resetType));
}