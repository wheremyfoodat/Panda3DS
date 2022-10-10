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

	logSVC("CreateEvent(handle pointer = %08X, resetType = %s)\n", outPointer, resetTypeToString(resetType));

	regs[0] = SVCResult::Success;
	regs[1] = makeEvent(static_cast<ResetType>(resetType));
}

// Result ClearEvent(Handle event)	
void Kernel::clearEvent() {
	const Handle handle = regs[0];
	const auto event = getObject(handle, KernelObjectType::Event);
	logSVC("ClearEvent(event handle = %d)\n", handle);

	if (event == nullptr) [[unlikely]] {
		regs[0] = SVCResult::BadHandle;
		return;
	}

	event->getData<Event>()->fired = false;
	regs[0] = SVCResult::Success;
}

// Result WaitSynchronization1(Handle handle, s64 timeout_nanoseconds)	
void Kernel::waitSynchronization1() {
	const Handle handle = regs[0];
	const s64 ns = s64(u64(regs[1]) | (u64(regs[2]) << 32));
	const auto event = getObject(handle, KernelObjectType::Event);

	if (event == nullptr) [[unlikely]] {
		Helpers::panic("WaitSynchronization1: Bad event handle");
		regs[0] = SVCResult::BadHandle;
		return;
	}

	logSVC("WaitSynchronization1(handle = %X, ns = %lld) (STUBBED)\n", handle, ns);
	regs[0] = SVCResult::Success;
}

// Result WaitSynchronizationN(s32* out, Handle* handles, s32 handlecount, bool waitAll, s64 timeout_nanoseconds)
void Kernel::waitSynchronizationN() {
	// TODO: Are these arguments even correct?
	u32 ns1 = regs[0];
	u32 handles = regs[1];
	u32 handleCount = regs[2];
	bool waitAll = regs[3] != 0;
	u32 ns2 = regs[4];
	u32 out = regs[5];

	logSVC("WaitSynchronizationN (STUBBED)\n");
	regs[0] = SVCResult::Success;

	if (currentThreadIndex == 1) {
		printf("WaitSynchN OoT hack triggered\n");
		switchThread(0);
	}
}