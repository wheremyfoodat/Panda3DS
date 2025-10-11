#include <bit>
#include <utility>

#include "cpu.hpp"
#include "kernel.hpp"

const char* Kernel::resetTypeToString(u32 type) {
	switch (type) {
		case 0: return "One shot";
		case 1: return "Sticky";
		case 2: return "Pulse";
		default: return "Invalid";
	}
}

HorizonHandle Kernel::makeEvent(ResetType resetType, Event::CallbackType callback) {
	Handle ret = makeObject(KernelObjectType::Event);
	objects[ret].data = new Event(resetType, callback);
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

	// Pulse events go back to being not fired once they are signaled
	if (event->resetType == ResetType::Pulse) {
		event->fired = false;
	}

	// Check if there's any thread waiting on this event
	if (event->waitlist != 0) {
		wakeupAllThreads(event->waitlist, handle);
		event->waitlist = 0;  // No threads waiting;

		if (event->resetType == ResetType::OneShot) {
			event->fired = false;
		}
	}

	rescheduleThreads();
	// Run the callback for events that require a special callback
	if (event->callback != Event::CallbackType::None) [[unlikely]] {
		runEventCallback(event->callback);
	}

	return true;
}

// Result CreateEvent(Handle* event, ResetType resetType)
void Kernel::svcCreateEvent() {
	const u32 outPointer = regs[0];
	const u32 resetType = regs[1];

	if (resetType > 2) {
		Helpers::panic("Invalid reset type for event %d", resetType);
	}

	logSVC("CreateEvent(handle pointer = %08X, resetType = %s)\n", outPointer, resetTypeToString(resetType));
	regs[0] = Result::Success;
	regs[1] = makeEvent(static_cast<ResetType>(resetType));
}

// Result ClearEvent(Handle event)
void Kernel::svcClearEvent() {
	const Handle handle = regs[0];
	const auto event = getObject(handle, KernelObjectType::Event);
	logSVC("ClearEvent(event handle = %X)\n", handle);

	if (event == nullptr) [[unlikely]] {
		Helpers::panic("Tried to clear non-existent event (handle = %X)", handle);
		regs[0] = Result::Kernel::InvalidHandle;
		return;
	}

	event->getData<Event>()->fired = false;
	regs[0] = Result::Success;
}

// Result SignalEvent(Handle event)
void Kernel::svcSignalEvent() {
	const Handle handle = regs[0];
	logSVC("SignalEvent(event handle = %X)\n", handle);
	KernelObject* object = getObject(handle, KernelObjectType::Event);

	if (object == nullptr) {
		Helpers::panic("Signalled non-existent event: %X\n", handle);
		regs[0] = Result::Kernel::InvalidHandle;
	} else {
		// We must signalEvent after setting r0, otherwise the r0 of the new thread will ne corrupted
		regs[0] = Result::Success;
		signalEvent(handle);
	}
}

// Result WaitSynchronization1(Handle handle, s64 timeout_nanoseconds)
void Kernel::waitSynchronization1() {
	const Handle handle = regs[0];
	const s64 ns = s64(u64(regs[2]) | (u64(regs[3]) << 32));
	logSVC("WaitSynchronization1(handle = %X, ns = %lld)\n", handle, ns);

	const auto object = getObject(handle);

	if (object == nullptr) [[unlikely]] {
		Helpers::panic("WaitSynchronization1: Bad event handle %X\n", handle);
		regs[0] = Result::Kernel::InvalidHandle;
		return;
	}

	if (!isWaitable(object)) [[unlikely]] {
		Helpers::panic("Tried to wait on a non waitable object. Type: %s, handle: %X\n", object->getTypeName(), handle);
	}

	if (!shouldWaitOnObject(object)) {
		acquireSyncObject(object, threads[currentThreadIndex]);  // Acquire the object since it's ready
		regs[0] = Result::Success;
	} else {
		// Timeout is 0, don't bother waiting, instantly timeout
		if (ns == 0) {
			regs[0] = Result::OS::Timeout;
			return;
		}

		regs[0] = Result::OS::Timeout;  // This will be overwritten with success if we don't timeout

		auto& t = threads[currentThreadIndex];
		t.waitList.resize(1);
		t.status = ThreadStatus::WaitSync1;
		t.wakeupTick = getWakeupTick(ns);
		t.waitList[0] = handle;

		// Add the current thread to the object's wait list
		object->getWaitlist() |= (1ull << currentThreadIndex);

		addWakeupEvent(t.wakeupTick);
		requireReschedule();
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
	s32 outPointer = regs[5];  // "out" pointer - shows which object got bonked if we're waiting on multiple objects
	s64 ns = s64(ns1) | (s64(ns2) << 32);

	logSVC("WaitSynchronizationN (handle pointer: %08X, count: %d, timeout = %lld)\n", handles, handleCount, ns);

	if (handleCount <= 0) {
		Helpers::panic("WaitSyncN: Invalid handle count");
	}

	// Temporary hack: Until we implement service sessions properly, don't bother sleeping when WaitSyncN targets a service handle
	// This is necessary because a lot of games use WaitSyncN with eg the CECD service
	if (handleCount == 1 && KernelHandles::isServiceHandle(mem.read32(handles))) {
		regs[0] = Result::Success;
		regs[1] = 0;
		return;
	}

	using WaitObject = std::pair<Handle, KernelObject*>;
	std::vector<WaitObject> waitObjects(handleCount);

	// We don't actually need to wait if waitAll == true unless one of the objects is not ready
	bool allReady = true;  // Default initialize to true, set to fault if one of the objects is not ready

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
			regs[0] = Result::Kernel::InvalidHandle;
			return;
		}

		// Panic if one of the objects is not a valid sync object
		if (!isWaitable(object)) [[unlikely]] {
			Helpers::panic("Tried to wait on a non waitable object in WaitSyncN. Type: %s, handle: %X\n", object->getTypeName(), handle);
		}

		if (shouldWaitOnObject(object)) {
			allReady = false;  // Derp, not all objects are ready :(
		} else {               /// At least one object is ready to be acquired ahead of time. If it's the first one, write it down
			if (!oneObjectReady) {
				oneObjectReady = true;
				firstReadyObjectIndex = i;
			}
		}

		waitObjects[i] = {handle, object};
	}

	auto& t = threads[currentThreadIndex];

	// We only need to wait on one object. Easy.
	if (!waitAll) {
		// If there's ready objects, acquire the first one and return
		if (oneObjectReady) {
			regs[0] = Result::Success;
			regs[1] = firstReadyObjectIndex;                                  // Return index of the acquired object
			acquireSyncObject(waitObjects[firstReadyObjectIndex].second, t);  // Acquire object
			return;
		}

		regs[0] = Result::OS::Timeout;  // This will be overwritten with success if we don't timeout
		// If the thread wakes up without timeout, this will be adjusted to the index of the handle that woke us up
		regs[1] = 0xFFFFFFFF;
		t.waitList.resize(handleCount);
		t.status = ThreadStatus::WaitSyncAny;
		t.outPointer = outPointer;
		t.wakeupTick = getWakeupTick(ns);

		for (s32 i = 0; i < handleCount; i++) {
			t.waitList[i] = waitObjects[i].first;                                  // Add object to this thread's waitlist
			waitObjects[i].second->getWaitlist() |= (1ull << currentThreadIndex);  // And add the thread to the object's waitlist
		}

		addWakeupEvent(t.wakeupTick);
		requireReschedule();
	} else {
		Helpers::panic("WaitSynchronizationN with waitAll");
	}
}

void Kernel::runEventCallback(Event::CallbackType callback) {
	switch (callback) {
		case Event::CallbackType::None: break;
		case Event::CallbackType::DSPSemaphore: serviceManager.getDSP().onSemaphoreEventSignal(); break;
		default: Helpers::panic("Unimplemented special callback for kernel event!"); break;
	}
}
