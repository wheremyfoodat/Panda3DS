#include <bit>
#include <cassert>
#include <cstring>

#include "arm_defs.hpp"
#include "kernel.hpp"
// This header needs to be included because I did stupid forward decl hack so the kernel and CPU can both access each
// other
#include "cpu.hpp"
#include "resource_limits.hpp"

// Switch to another thread
// newThread: Index of the newThread in the thread array (NOT a handle).
void Kernel::switchThread(int newThreadIndex) {
	auto& oldThread = threads[currentThreadIndex];
	auto& newThread = threads[newThreadIndex];
	newThread.status = ThreadStatus::Running;
	logThread("Switching from thread %d to %d\n", currentThreadIndex, newThreadIndex);

	// Bail early if the new thread is actually the old thread
	if (currentThreadIndex == newThreadIndex) [[unlikely]] {
		return;
	}

	// Backup context
	std::memcpy(oldThread.gprs.data(), cpu.regs().data(), cpu.regs().size_bytes());  // Backup the 16 GPRs
	std::memcpy(oldThread.fprs.data(), cpu.fprs().data(), cpu.fprs().size_bytes());  // Backup the 32 FPRs
	oldThread.cpsr = cpu.getCPSR();                                                  // Backup CPSR
	oldThread.fpscr = cpu.getFPSCR();                                                // Backup FPSCR

	// Load new context
	std::memcpy(cpu.regs().data(), newThread.gprs.data(), cpu.regs().size_bytes());  // Load 16 GPRs
	std::memcpy(cpu.fprs().data(), newThread.fprs.data(), cpu.fprs().size_bytes());  // Load 32 FPRs
	cpu.setCPSR(newThread.cpsr);                                                     // Load CPSR
	cpu.setFPSCR(newThread.fpscr);                                                   // Load FPSCR
	cpu.setTLSBase(newThread.tlsBase);  // Load CP15 thread-local-storage pointer register

	currentThreadIndex = newThreadIndex;
}

// Sort the threadIndices vector based on the priority of each thread
// The threads with higher priority (aka the ones with a lower priority value) should come first in the vector
void Kernel::sortThreads() {
	std::vector<int>& v = threadIndices;
	std::sort(v.begin(), v.end(), [&](int a, int b) {
		return threads[a].priority < threads[b].priority;
	});
}

bool Kernel::canThreadRun(const Thread& t) {
	if (t.status == ThreadStatus::Ready) {
		return true;
	} else if (t.status == ThreadStatus::WaitSleep || t.status == ThreadStatus::WaitSync1
		|| t.status == ThreadStatus::WaitSyncAny || t.status == ThreadStatus::WaitSyncAll) {
		const u64 elapsedTicks = cpu.getTicks() - t.sleepTick;

		constexpr double ticksPerSec = double(CPU::ticksPerSec);
		constexpr double nsPerTick = ticksPerSec / 1000000000.0;

		// TODO: Set r0 to the correct error code on timeout for WaitSync{1/Any/All}
		const s64 elapsedNs = s64(double(elapsedTicks) * nsPerTick);
		return elapsedNs >= t.waitingNanoseconds;
	}

	// Handle timeouts and stuff here
	return false;
}

// Get the index of the next thread to run by iterating through the thread list and finding the free thread with the highest priority
// Returns the thread index if a thread is found, or nullopt otherwise
std::optional<int> Kernel::getNextThread() {
	for (auto index : threadIndices) {
		const Thread& t = threads[index];

		// Thread is ready, return it
		if (canThreadRun(t)) {
			return index;
		}
	}

	// No thread was found
	return std::nullopt;
}

// See if there is a higher priority, ready thread and switch to that
void Kernel::rescheduleThreads() {
	Thread& current = threads[currentThreadIndex];  // Current running thread

	// If the current thread is running and hasn't gone to sleep or whatever, set it to Ready instead of Running
	// So that getNextThread will evaluate it properly
	if (current.status == ThreadStatus::Running) {
		current.status = ThreadStatus::Ready;
	}
	ThreadStatus currentStatus = current.status;
	std::optional<int> newThreadIndex = getNextThread();

	// Case 1: A thread can run
	if (newThreadIndex.has_value()) {
		switchThread(newThreadIndex.value());
	} 
	
	// Case 2: No other thread can run, straight to the idle thread
	else {
		switchThread(idleThreadIndex);
	}
}

// Internal OS function to spawn a thread
Handle Kernel::makeThread(u32 entrypoint, u32 initialSP, u32 priority, ProcessorID id, u32 arg, ThreadStatus status) {
	int index; // Index of the created thread in the  threads array

	if (threadCount < appResourceLimits.maxThreads) [[likely]] { // If we have not yet created over too many threads
		index = threadCount++;
	} else if (aliveThreadCount < appResourceLimits.maxThreads) { // If we have created many threads but at least one is dead & reusable
		for (int i = 0; i < threads.size(); i++) {
			if (threads[i].status == ThreadStatus::Dead) {
				index = i;
				break;
			}
		}
	} else { // There is no thread we can use, we're screwed
		Helpers::panic("Overflowed thread count!!");
	}

	aliveThreadCount++;

	threadIndices.push_back(index);
	Thread& t = threads[index]; // Reference to thread data
	Handle ret = makeObject(KernelObjectType::Thread);
	objects[ret].data = &t;

	const bool isThumb = (entrypoint & 1) != 0; // Whether the thread starts in thumb mode or not

	// Set up initial thread context
	t.gprs.fill(0);
	t.fprs.fill(0);

	t.arg = arg;
	t.initialSP = initialSP;
	t.entrypoint = entrypoint;

	t.gprs[0] = arg;
	t.gprs[13] = initialSP;
	t.gprs[15] = entrypoint;
	t.priority = priority;
	t.processorID = id;
	t.status = status;
	t.handle = ret;
	t.waitingAddress = 0;
	t.threadsWaitingForTermination = 0; // Thread just spawned, no other threads waiting for it to terminate

	t.cpsr = CPSR::UserMode | (isThumb ? CPSR::Thumb : 0);
	t.fpscr = FPSCR::ThreadDefault;
	// Initial TLS base has already been set in Kernel::Kernel()
	// TODO: Does svcCreateThread zero-set the TLS of the new thread?

	sortThreads();
	return ret;
}

Handle Kernel::makeMutex(bool locked) {
	Handle ret = makeObject(KernelObjectType::Mutex);
	objects[ret].data = new Mutex(locked, ret);

	// If the mutex is initially locked, store the index of the thread that owns it and set lock count to 1
	if (locked) {
		Mutex* moo = objects[ret].getData<Mutex>();
		moo->ownerThread = currentThreadIndex;
	}

	// Push the new mutex to our list of mutex handles
	// We need a list of mutex handles so that when a thread is killed, we can look which mutexes from this list the thread owns and free them
	// Alternatively this could be a per-thread list, but I don't want to push_back and remove on every mutex lock and release
	// Since some mutexes like the APT service mutex are locked and unlocked constantly, while ExitThread is a relatively "rare" SVC
	mutexHandles.push_back(ret);
	return ret;
}

void Kernel::releaseMutex(Mutex* moo) {
	// TODO: Assert lockCount > 0 before release, maybe. The SVC should be safe at least.
	moo->lockCount--; // Decrement lock count

	// If the lock count reached 0 then the thread no longer owns the mootex and it can be given to a new one
	if (moo->lockCount == 0) {
		moo->locked = false;

		if (moo->waitlist != 0) {
			int index = wakeupOneThread(moo->waitlist, moo->handle); // Wake up one thread and get its index
			moo->waitlist ^= (1ull << index); // Remove thread from waitlist

			// Have new thread acquire mutex
			moo->locked = true;
			moo->lockCount = 1;
			moo->ownerThread = index;
		}

		requireReschedule();
	}
}

Handle Kernel::makeSemaphore(u32 initialCount, u32 maximumCount) {
	Handle ret = makeObject(KernelObjectType::Semaphore);
	objects[ret].data = new Semaphore(initialCount, maximumCount);

	return ret;
}

void Kernel::sleepThreadOnArbiter(u32 waitingAddress) {
	Thread& t = threads[currentThreadIndex];
	t.status = ThreadStatus::WaitArbiter;
	t.waitingAddress = waitingAddress;

	requireReschedule();
}

// Acquires an object that is **ready to be acquired** without waiting on it
void Kernel::acquireSyncObject(KernelObject* object, const Thread& thread) {
	switch (object->type) {
		case KernelObjectType::Event: {
			Event* e = object->getData<Event>();
			if (e->resetType == ResetType::OneShot) { // One-shot events automatically get cleared after waking up a thread
				e->fired = false;
			}
			break;
		}

		case KernelObjectType::Mutex: {
			Mutex* moo = object->getData<Mutex>();

			// Only reschedule if we're acquiring the mutex for the first time
			if (!moo->locked) {
				moo->locked = true;
				requireReschedule();
			}

			// Increment lock count by 1. If a thread acquires a mootex multiple times, it needs to release it until count == 0
			// For the mootex to be free.
			moo->lockCount++;
			moo->ownerThread = thread.index;
			break;
		}

		case KernelObjectType::Semaphore: {
			Semaphore* s = object->getData<Semaphore>();
			if (s->availableCount <= 0) [[unlikely]] // This should be unreachable but let's check anyways
				Helpers::panic("Tried to acquire unacquirable semaphore");

			s->availableCount -= 1;
			break;
		}

		case KernelObjectType::Thread:
			break;

		case KernelObjectType::Timer: {
			Timer* timer = object->getData<Timer>();
			if (timer->resetType == ResetType::OneShot) {  // One-shot timers automatically get cleared after waking up a thread
				timer->fired = false;
			}
			break;
		}

		default: Helpers::panic("Acquiring unimplemented sync object %s", object->getTypeName());
	}
}

// Wake up one of the threads in the waitlist (the one with highest prio) and return its index
// Must not be called with an empty waitlist
int Kernel::wakeupOneThread(u64 waitlist, Handle handle) {
	if (waitlist == 0) [[unlikely]]
		Helpers::panic("[Internal error] It shouldn't be possible to call wakeupOneThread when there's 0 threads waiting!");

	// Find the waiting thread with the highest priority.
	// We do this by first picking the first thread in the waitlist, then checking each other thread and comparing priority
	int threadIndex = std::countr_zero(waitlist);    // Index of first thread
	int maxPriority = threads[threadIndex].priority; // Set initial max prio to the prio of the first thread
	waitlist ^= (1ull << threadIndex); // Remove thread from the waitlist

	while (waitlist != 0) {
		int newThread = std::countr_zero(waitlist); // Get new thread and evaluate whether it has a higher priority
		if (threads[newThread].priority < maxPriority) { // Low priority value means high priority
			threadIndex = newThread;
			maxPriority = threads[newThread].priority;
		}

		waitlist ^= (1ull << threadIndex); // Remove thread from waitlist
	}

	Thread& t = threads[threadIndex];
	switch (t.status) {
		case ThreadStatus::WaitSync1:
			t.status = ThreadStatus::Ready;
			t.gprs[0] = Result::Success; // The thread did not timeout, so write success to r0
			break;

		case ThreadStatus::WaitSyncAny:
			t.status = ThreadStatus::Ready;
			t.gprs[0] = Result::Success; // The thread did not timeout, so write success to r0

			// Get the index of the event in the object's waitlist, write it to r1
			for (size_t i = 0; i < t.waitList.size(); i++) {
				if (t.waitList[i] == handle) {
					t.gprs[1] = u32(i);
					break;
				}
			}
			break;

		case ThreadStatus::WaitSyncAll:
			Helpers::panic("WakeupOneThread: Thread on WaitSyncAll");
			break;
	}

	return threadIndex;
}

// Wake up every single thread in the waitlist using a bit scanning algorithm
void Kernel::wakeupAllThreads(u64 waitlist, Handle handle) {
	while (waitlist != 0) {
		const uint index = std::countr_zero(waitlist); // Get one of the set bits to see which thread is waiting
		waitlist ^= (1ull << index); // Remove thread from waitlist by toggling its bit

		// Get the thread we'll be signalling
		Thread& t = threads[index];
		switch (t.status) {
		case ThreadStatus::WaitSync1:
			t.status = ThreadStatus::Ready;
			t.gprs[0] = Result::Success; // The thread did not timeout, so write success to r0
			break;

		case ThreadStatus::WaitSyncAny:
			t.status = ThreadStatus::Ready;
			t.gprs[0] = Result::Success; // The thread did not timeout, so write success to r0

			// Get the index of the event in the object's waitlist, write it to r1
			for (size_t i = 0; i < t.waitList.size(); i++) {
				if (t.waitList[i] == handle) {
					t.gprs[1] = u32(i);
					break;
				}
			}
			break;

		case ThreadStatus::WaitSyncAll:
			Helpers::panic("WakeupAllThreads: Thread on WaitSyncAll");
			break;
		}
	}
}

// Make a thread sleep for a certain amount of nanoseconds at minimum
void Kernel::sleepThread(s64 ns) {
	if (ns < 0) {
		Helpers::panic("Sleeping a thread for a negative amount of ns");
	} else if (ns == 0) {
		// TODO: This is garbage, but it works so eh we can keep it for now
		Thread& t = threads[currentThreadIndex];

		// See if a thread other than this and the idle thread is waiting to run by temp marking the current function as dead and searching
		// If there is another thread to run, then run it. Otherwise, go back to this thread, not to the idle thread
		t.status = ThreadStatus::Dead;
		auto nextThreadIndex = getNextThread();
		t.status = ThreadStatus::Ready;

		if (nextThreadIndex.has_value()) {
			const auto index = nextThreadIndex.value();

			if (index != idleThreadIndex) {
				switchThread(index);
			}
		}
	} else {  // If we're sleeping for >= 0 ns
		Thread& t = threads[currentThreadIndex];

		t.status = ThreadStatus::WaitSleep;
		t.waitingNanoseconds = ns;
		t.sleepTick = cpu.getTicks();

		requireReschedule();
	}
}

// Result CreateThread(s32 priority, ThreadFunc entrypoint, u32 arg, u32 stacktop, s32 threadPriority, s32 processorID)
void Kernel::createThread() {
	u32 priority = regs[0];
	u32 entrypoint = regs[1];
	u32 arg = regs[2]; // An argument value stored in r0 of the new thread
	u32 initialSP = regs[3] & ~7; // SP is force-aligned to 8 bytes
	s32 id = static_cast<s32>(regs[4]);

	logSVC("CreateThread(entry = %08X, stacktop = %08X, arg = %X, priority = %X, processor ID = %d)\n", entrypoint,
		initialSP, arg, priority, id);

	if (priority > 0x3F) [[unlikely]] {
		Helpers::panic("Created thread with bad priority value %X", priority);
		regs[0] = Result::OS::OutOfRange;
		return;
	}

	if (id < -2 || id > 3) {
		Helpers::panic("Invalid processor ID in CreateThread");
	}

	regs[0] = Result::Success;
	regs[1] = makeThread(entrypoint, initialSP, priority, static_cast<ProcessorID>(id), arg, ThreadStatus::Ready);
	requireReschedule();
}

// void SleepThread(s64 nanoseconds)
void Kernel::svcSleepThread() {
	const s64 ns = s64(u64(regs[0]) | (u64(regs[1]) << 32));
	//logSVC("SleepThread(ns = %lld)\n", ns);

	regs[0] = Result::Success;
	sleepThread(ns);
}

void Kernel::getThreadID() {
	Handle handle = regs[1];
	logSVC("GetThreadID(handle = %X)\n", handle);

	if (handle == KernelHandles::CurrentThread) {
		regs[0] = Result::Success;
		regs[1] = currentThreadIndex;
		return;
	}

	const auto thread = getObject(handle, KernelObjectType::Thread);
	if (thread == nullptr) [[unlikely]] {
		regs[0] = Result::Kernel::InvalidHandle;
		return;
	}

	regs[0] = Result::Success;
	regs[1] = thread->getData<Thread>()->index;
}

void Kernel::getThreadPriority() {
	const Handle handle = regs[1];
	logSVC("GetThreadPriority (handle = %X)\n", handle);

	if (handle == KernelHandles::CurrentThread) {
		regs[0] = Result::Success;
		regs[1] = threads[currentThreadIndex].priority;
	} else {
		auto object = getObject(handle, KernelObjectType::Thread);
		if (object == nullptr) [[unlikely]] {
			regs[0] = Result::Kernel::InvalidHandle;
		} else {
			regs[0] = Result::Success;
			regs[1] = object->getData<Thread>()->priority;
		}
	}
}

void Kernel::getThreadIdealProcessor() {
	const Handle handle = regs[1];  // Thread handle
	logSVC("GetThreadIdealProcessor (handle = %X)\n", handle);

	// TODO: Not documented what this is or what it does. Citra doesn't implement it at all. Return AppCore as the ideal processor for now
	regs[0] = Result::Success;
	regs[1] = static_cast<u32>(ProcessorID::AppCore);
}

void Kernel::setThreadPriority() {
	const Handle handle = regs[0];
	const u32 priority = regs[1];
	logSVC("SetThreadPriority (handle = %X, priority = %X)\n", handle, priority);

	if (priority > 0x3F) {
		regs[0] = Result::OS::OutOfRange;
		return;
	}

	if (handle == KernelHandles::CurrentThread) {
		regs[0] = Result::Success;
		threads[currentThreadIndex].priority = priority;
	} else {
		auto object = getObject(handle, KernelObjectType::Thread);
		if (object == nullptr) [[unlikely]] {
			regs[0] = Result::Kernel::InvalidHandle;
			return;
		} else {
			regs[0] = Result::Success;
			object->getData<Thread>()->priority = priority;
		}
	}
	sortThreads();
	requireReschedule();
}

void Kernel::getCurrentProcessorNumber() {
	logSVC("GetCurrentProcessorNumber()\n");
	const ProcessorID id = threads[currentThreadIndex].processorID;
	s32 ret;

	// Until we properly implement per-core schedulers, return whatever processor ID passed to svcCreateThread
	switch (id) {
		// TODO: This is picked from exheader
		case ProcessorID::Default:
			ret = static_cast<s32>(ProcessorID::AppCore);
			break;

		case ProcessorID::AllCPUs:
			ret = static_cast<s32>(ProcessorID::AppCore);
			Helpers::warn("GetCurrentProcessorNumber on thread created to run on all CPUs...?\n");
			break;

		default: ret = static_cast<s32>(id); break;
	}

	if (ret != static_cast<s32>(ProcessorID::AppCore)) {
		Helpers::warn("GetCurrentProcessorNumber: Thread not running on appcore\n");
	}

	regs[0] = static_cast<u32>(ret);
}

void Kernel::exitThread() {
	logSVC("ExitThread\n");

	// Find which mutexes this thread owns, release them
	for (auto handle : mutexHandles) {
		KernelObject* object = getObject(handle, KernelObjectType::Mutex);

		// Make sure that the handle actually matches to a mutex, and if our exiting thread owns the mutex, release it
		if (object != nullptr) {
			Mutex* moo = object->getData<Mutex>();

			if (moo->locked && moo->ownerThread == currentThreadIndex) {
				// Release the mutex by setting lock count to 1 and releasing it once. We set lock count to 1 since it's a recursive mutex
				// Therefore if its lock count was > 1, simply calling releaseMutex would not fully release it
				moo->lockCount = 1;
				releaseMutex(moo);
			}
		}
	}

	// Remove the index of this thread from the thread indices vector
	for (int i = 0; i < threadIndices.size(); i++) {
		if (threadIndices[i] == currentThreadIndex)
			threadIndices.erase(threadIndices.begin() + i);
	}

	Thread& t = threads[currentThreadIndex];
	t.status = ThreadStatus::Dead;
	aliveThreadCount--;

	// Check if any threads are sleeping, waiting for this thread to terminate, and wake them up
	// This is how thread joining is implemented in the kernel - you wait on a thread, like any other wait object.
	if (t.threadsWaitingForTermination != 0) {
		// TODO: Handle cloned handles? Not sure how those interact with wait object signalling
		wakeupAllThreads(t.threadsWaitingForTermination, t.handle);
		t.threadsWaitingForTermination = 0; // No other threads waiting
	}

	requireReschedule();
}

void Kernel::svcCreateMutex() {
	bool locked = regs[1] != 0;
	logSVC("CreateMutex (locked = %s)\n", locked ? "yes" : "no");

	regs[0] = Result::Success;
	regs[1] = makeMutex(locked);
}

void Kernel::svcReleaseMutex() {
	const Handle handle = regs[0];
	logSVC("ReleaseMutex (handle = %x)\n", handle);

	const auto object = getObject(handle, KernelObjectType::Mutex);
	if (object == nullptr) [[unlikely]] {
		Helpers::panic("Tried to release non-existent mutex");
		regs[0] = Result::Kernel::InvalidHandle;
		return;
	}

	Mutex* moo = object->getData<Mutex>();
	// A thread can't release a mutex it does not own
	if (!moo->locked || moo->ownerThread != currentThreadIndex) {
		regs[0] = Result::Kernel::InvalidMutexRelease;
		return;
	}

	regs[0] = Result::Success;
	releaseMutex(moo);
}

void Kernel::svcCreateSemaphore() {
	s32 initialCount = static_cast<s32>(regs[1]);
	s32 maxCount = static_cast<s32>(regs[2]);
	logSVC("CreateSemaphore (initial count = %d, max count = %d)\n", initialCount, maxCount);

	if (initialCount > maxCount)
		Helpers::panic("CreateSemaphore: Initial count higher than max count");

	if (initialCount < 0 || maxCount < 0)
		Helpers::panic("CreateSemaphore: Negative count value");

	regs[0] = Result::Success;
	regs[1] = makeSemaphore(initialCount, maxCount);
}

void Kernel::svcReleaseSemaphore() {
	const Handle handle = regs[1];
	const s32 releaseCount = static_cast<s32>(regs[2]);
	logSVC("ReleaseSemaphore (handle = %X, release count = %d)\n", handle, releaseCount);

	const auto object = getObject(handle, KernelObjectType::Semaphore);
	if (object == nullptr) [[unlikely]] {
		Helpers::panic("Tried to release non-existent semaphore");
		regs[0] = Result::Kernel::InvalidHandle;
		return;
	}

	if (releaseCount < 0)
		Helpers::panic("ReleaseSemaphore: Negative count");

	Semaphore* s = object->getData<Semaphore>();
	if (s->maximumCount - s->availableCount < releaseCount)
		Helpers::panic("ReleaseSemaphore: Release count too high");

	// Write success and old available count to r0 and r1 respectively
	regs[0] = Result::Success;
	regs[1] = s->availableCount;
	// Bump available count
	s->availableCount += releaseCount;

	// Wake up threads one by one until the available count hits 0 or we run out of threads to wake up
	while (s->availableCount > 0 && s->waitlist != 0) {
		int index = wakeupOneThread(s->waitlist, handle); // Wake up highest priority thread
		s->waitlist ^= (1ull << index); // Remove thread from waitlist

		s->availableCount--; // Decrement available count
	}
}

// Returns whether an object is waitable or not
// The KernelObject type enum is arranged in a specific order in kernel_types.hpp so this
// can simply compile to a fast sub+cmp+set despite looking slow
bool Kernel::isWaitable(const KernelObject* object) {
	auto type = object->type;
	return type == KernelObjectType::Event || type == KernelObjectType::Mutex || type == KernelObjectType::Port ||
		   type == KernelObjectType::Semaphore || type == KernelObjectType::Timer || type == KernelObjectType::Thread;
}

// Returns whether we should wait on a sync object or not
bool Kernel::shouldWaitOnObject(KernelObject* object) {
	switch (object->type) {
		case KernelObjectType::Event: // We should wait on an event only if it has not been signalled
			return !object->getData<Event>()->fired;

		case KernelObjectType::Mutex: {
			Mutex* moo = object->getData<Mutex>(); // mooooooooooo
			return moo->locked && moo->ownerThread != currentThreadIndex; // If the current thread owns the moo then no reason to wait
		}

		case KernelObjectType::Thread: // Waiting on a thread waits until it's dead. If it's dead then no need to wait
			return object->getData<Thread>()->status != ThreadStatus::Dead;

		case KernelObjectType::Timer: // We should wait on a timer only if it has not been signalled
			return !object->getData<Timer>()->fired;

		case KernelObjectType::Semaphore: // Wait if the semaphore count <= 0
			return object->getData<Semaphore>()->availableCount <= 0;

		default:
			Helpers::panic("Not sure whether to wait on object (type: %s)", object->getTypeName());
			return true;
	}
}
