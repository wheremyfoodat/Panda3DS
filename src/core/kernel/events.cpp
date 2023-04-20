#include "kernel.hpp"
#include "cpu.hpp"
#include <utility>

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

bool Kernel::signalEvent(Handle handle) {
	KernelObject* object = getObject(handle, KernelObjectType::Event);
	if (object == nullptr) [[unlikely]] {
		Helpers::panic("Tried to signal non-existent event");
		return false;
	}

	Event* event = object->getData<Event>();
	event->fired = true;

	if (event->waitlist != 0) {
		Helpers::panic("Tried to signal event with a waitlist");
	}
	/*
	switch (event->resetType) {
		case ResetType::OneShot:
			for (int i = 0; i < threadCount; i++) {
				Thread& t = threads[i];

				if (t.status == ThreadStatus::WaitSync1 && t.waitList[0] == handle) {
					t.status = ThreadStatus::Ready;
					break;
				}
				else if (t.status == ThreadStatus::WaitSyncAll) {
					Helpers::panic("Trying to SignalEvent when a thread is waiting on multiple objects");
				}
			}
			break;

		default:
			Helpers::panic("Signaled event of unimplemented type: %d", event->resetType);
	}
	*/

	return true;
}

// Result CreateEvent(Handle* event, ResetType resetType)
void Kernel::svcCreateEvent() {
	const u32 outPointer = regs[0];
	const u32 resetType = regs[1];

	if (resetType > 2)
		Helpers::panic("Invalid reset type for event %d", resetType);

	logSVC("CreateEvent(handle pointer = %08X, resetType = %s)\n", outPointer, resetTypeToString(resetType));

	regs[0] = SVCResult::Success;
	regs[1] = makeEvent(static_cast<ResetType>(resetType));
}

// Result ClearEvent(Handle event)	
void Kernel::svcClearEvent() {
	const Handle handle = regs[0];
	const auto event = getObject(handle, KernelObjectType::Event);
	logSVC("ClearEvent(event handle = %X)\n", handle);

	if (event == nullptr) [[unlikely]] {
		Helpers::panic("Tried to clear non-existent event (handle = %X)", handle);
		regs[0] = SVCResult::BadHandle;
		return;
	}

	event->getData<Event>()->fired = false;
	regs[0] = SVCResult::Success;
}

// Result SignalEvent(Handle event)	
void Kernel::svcSignalEvent() {
	const Handle handle = regs[0];
	logSVC("SignalEvent(event handle = %X)\n", handle);
	
	if (!signalEvent(handle)) {
		Helpers::panic("Signalled non-existent event: %X\n", handle);
		regs[0] = SVCResult::BadHandle;
	} else {
		regs[0] = SVCResult::Success;
	}
}

// Result WaitSynchronization1(Handle handle, s64 timeout_nanoseconds)	
void Kernel::waitSynchronization1() {
	const Handle handle = regs[0];
	const s64 ns = s64(u64(regs[1]) | (u64(regs[2]) << 32));
	logSVC("WaitSynchronization1(handle = %X, ns = %lld)\n", handle, ns);

	const auto object = getObject(handle);

	if (object == nullptr) [[unlikely]] {
		Helpers::panic("WaitSynchronization1: Bad event handle %X\n", handle);
		regs[0] = SVCResult::BadHandle;
		return;
	}

	if (!isWaitable(object)) [[unlikely]] {
		Helpers::panic("Tried to wait on a non waitable object. Type: %s, handle: %X\n", object->getTypeName(), handle);
	}

	if (!shouldWaitOnObject(object)) {
		acquireSyncObject(object, threads[currentThreadIndex]); // Acquire the object since it's ready
		regs[0] = SVCResult::Success;
	} else {
		// Timeout is 0, don't bother waiting, instantly timeout
		if (ns == 0) {
			regs[0] = SVCResult::Timeout;
			return;
		}

		regs[0] = SVCResult::Success;

		auto& t = threads[currentThreadIndex];
		t.waitList.resize(1);
		t.status = ThreadStatus::WaitSync1;
		t.sleepTick = cpu.getTicks();
		t.waitingNanoseconds = ns;
		t.waitList[0] = handle;

		// Add the current thread to the object's wait list
		object->getWaitlist() |= (1ull << currentThreadIndex);

		switchToNextThread();
	}
}

// Result WaitSynchronizationN(s32* out, Handle* handles, s32 handlecount, bool waitAll, s64 timeout_nanoseconds)
void Kernel::waitSynchronizationN() {
	// TODO: Are these arguments even correct?
	s32 ns1 = regs[0];
	u32 handles = regs[1];
	s32 handleCount = regs[2];
	bool waitAll = regs[3] != 0;
	u32 ns2 = regs[4];
	s32 outPointer = regs[5]; // "out" pointer - shows which object got bonked if we're waiting on multiple objects
	s64 ns = s64(ns1) | (s64(ns2) << 32);

	logSVC("WaitSynchronizationN (handle pointer: %08X, count: %d, timeout = %lld)\n", handles, handleCount, ns);

	if (waitAll && handleCount > 1)
		Helpers::panic("Trying to wait on more than 1 object");

	if (handleCount < 0)
		Helpers::panic("WaitSyncN: Invalid handle count");

	using WaitObject = std::pair<Handle, KernelObject*>;
	std::vector<WaitObject> waitObjects(handleCount);

	// We don't actually need to wait if waitAll == true unless one of the objects is not ready
	bool allReady = true; // Default initialize to true, set to fault if one of the objects is not ready

	// Tracks whether at least one object is ready, + the index of the first ready object
	// This is used when waitAll == false, because if one object is already available then we can skip the sleeping
	bool oneObjectReady = false;
	s32 firstReadyObjectIndex = 0;

	for (s32 i = 0; i < handleCount; i++) {
		Handle handle = mem.read32(handles);
		handles += sizeof(Handle);

		auto object = getObject(handle);
		// Panic if one of the objects is not even an object
		if (object == nullptr) [[unlikely]] {
			Helpers::panic("WaitSynchronizationN: Bad object handle %X\n", handle);
			regs[0] = SVCResult::BadHandle;
			return;
		}

		// Panic if one of the objects is not a valid sync object
		if (!isWaitable(object)) [[unlikely]] {
			Helpers::panic("Tried to wait on a non waitable object in WaitSyncN. Type: %s, handle: %X\n",
				object->getTypeName(), handle);
		}

		if (shouldWaitOnObject(object)) {
			allReady = false; // Derp, not all objects are ready :(
		} else { /// At least one object is ready to be acquired ahead of time. If it's the first one, write it down
			if (!oneObjectReady) {
				oneObjectReady = true;
				firstReadyObjectIndex = i;
			}
		}

		waitObjects[i] = {handle, object};
	}

	auto& t = threads[currentThreadIndex];

	// We only need to wait on one object. Easy...?!
	if (!waitAll) {
		// If there's ready objects, acquire the first one and return 
		if (oneObjectReady) {
			regs[0] = SVCResult::Success;
			regs[1] = firstReadyObjectIndex; // Return index of the acquired object
			acquireSyncObject(waitObjects[firstReadyObjectIndex].second, t); // Acquire object
			return;
		}

		regs[0] = SVCResult::Success; // If the thread times out, this should be adjusted to SVCResult::Timeout
		regs[1] = handleCount - 1; // When the thread exits, this will be adjusted to mirror which handle woke us up
		t.waitList.resize(handleCount);
		t.status = ThreadStatus::WaitSyncAny;
		t.outPointer = outPointer;
		t.waitingNanoseconds = ns;
		t.sleepTick = cpu.getTicks();
		
		for (s32 i = 0; i < handleCount; i++) {
			t.waitList[i] = waitObjects[i].first; // Add object to this thread's waitlist
			waitObjects[i].second->getWaitlist() |= (1ull << currentThreadIndex); // And add the thread to the object's waitlist
		}

		switchToNextThread();
	} else {
		Helpers::panic("WaitSynchronizatioN with waitAll");
	}
}