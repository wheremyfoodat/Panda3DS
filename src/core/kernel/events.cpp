#include "kernel.hpp"

// TODO: What the fuck is resetType meant to be?
Handle Kernel::makeEvent(u32 resetType) {
	Handle ret = makeObject(KernelObjectType::Event);
	return ret;
}

// Result CreateEvent(Handle* event, ResetType resetType)
// TODO: Just like getResourceLimit this seems to output the handle in r1 even though 3dbrew doesn't mention this
// Should the handle be written both in memory and r1, or just r1?
void Kernel::createEvent() {
	const u32 outPointer = regs[0];
	const u32 resetType = regs[1];
	printf("CreateEvent(handle pointer = %08X, resetType = %d)\n", outPointer, resetType);

	Handle handle = makeEvent(resetType);
	regs[0] = SVCResult::Success;
	regs[1] = handle;
	mem.write32(outPointer, handle);
}