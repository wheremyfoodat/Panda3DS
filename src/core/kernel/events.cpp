#include "kernel.hpp"
#include "cpu.hpp"

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
	logSVC("ClearEvent(event handle = %X)\n", handle);

	if (event == nullptr) [[unlikely]] {
		regs[0] = SVCResult::BadHandle;
		return;
	}

	event->getData<Event>()->fired = false;
	regs[0] = SVCResult::Success;
}

// Result SignalEvent(Handle event)	
void Kernel::signalEvent() {
	const Handle handle = regs[0];
	const auto event = getObject(handle, KernelObjectType::Event);
	logSVC("SignalEvent(event handle = %X)\n", handle);
	printf("Stubbed SignalEvent!!\n");

	/*
		if (event == nullptr) [[unlikely]] {
			regs[0] = SVCResult::BadHandle;
			return;
		}

		event->getData<Event>()->fired = true;
	*/

	regs[0] = SVCResult::Success;
}

// Result WaitSynchronization1(Handle handle, s64 timeout_nanoseconds)	
void Kernel::waitSynchronization1() {
	const Handle handle = regs[0];
	const s64 ns = s64(u64(regs[1]) | (u64(regs[2]) << 32));
	logSVC("WaitSynchronization1(handle = %X, ns = %lld)\n", handle, ns);

	const auto object = getObject(handle);

	if (object == nullptr) [[unlikely]] {
		Helpers::panic("WaitSynchronization1: Bad event handle");
		regs[0] = SVCResult::BadHandle;
		return;
	}

	if (!isWaitable(object)) [[unlikely]] {
		Helpers::panic("Tried to wait on a non waitable object. Type: %s, handle: %X\n", object->getTypeName(), handle);
	}

	regs[0] = SVCResult::Success;

	auto& t = threads[currentThreadIndex];
	t.waitList.resize(1);
	t.status = ThreadStatus::WaitSync1;
	t.sleepTick = cpu.getTicks();
	t.waitingNanoseconds = ns;
	t.waitList[0] = handle;
	switchToNextThread();
}

// Result WaitSynchronizationN(s32* out, Handle* handles, s32 handlecount, bool waitAll, s64 timeout_nanoseconds)
void Kernel::waitSynchronizationN() {
	// TODO: Are these arguments even correct?
	s32 ns1 = regs[0];
	u32 handles = regs[1];
	u32 handleCount = regs[2];
	bool waitAll = regs[3] != 0;
	u32 ns2 = regs[4];
	s32 pointer = regs[5];
	s64 ns = s64(ns1) | (s64(ns2) << 32);

	logSVC("WaitSynchronizationN (handle pointer: %08X, count: %d)\n", handles, handleCount);
	ThreadStatus newStatus = waitAll ? ThreadStatus::WaitSyncAll : ThreadStatus::WaitSync1;

	auto& t = threads[currentThreadIndex];
	t.waitList.resize(handleCount);
	
	for (uint i = 0; i < handleCount; i++) {
		Handle handle = mem.read32(handles);
		handles += sizeof(Handle);

		t.waitList[i] = handle;

		auto object = getObject(handle);
		if (object == nullptr) [[unlikely]] {
			Helpers::panic("WaitSynchronizationN: Bad object handle");
			regs[0] = SVCResult::BadHandle;
			return;
		}

		if (!isWaitable(object)) [[unlikely]] {
			Helpers::panic("Tried to wait on a non waitable object. Type: %s, handle: %X\n", object->getTypeName(), handle);
		}
	}

	regs[0] = SVCResult::Success;
	t.status = newStatus;
	t.waitingNanoseconds = ns;
	switchToNextThread();
}