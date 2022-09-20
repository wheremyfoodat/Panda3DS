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

// Result ClearEvent(Handle event)	
void Kernel::clearEvent() {
	const Handle handle = regs[0];
	const auto event = getObject(handle, KernelObjectType::Event);
	printf("ClearEvent(event handle = %d)\n", handle);

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

	printf("WaitSynchronization1(handle = %X, ns = %lld) (STUBBED)\n", handle, ns);
	serviceManager.requestGPUInterrupt(GPUInterrupt::VBlank0);
	serviceManager.requestGPUInterrupt(GPUInterrupt::VBlank1);
	regs[0] = SVCResult::Success;
}