#include <cstring>
#include "kernel.hpp"
#include "arm_defs.hpp"
// This header needs to be included because I did stupid forward decl hack so the kernel and CPU can both access each other
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
	std::memcpy(&oldThread.gprs[0], &cpu.regs()[0], 16 * sizeof(u32)); // Backup the 16 GPRs
	std::memcpy(&oldThread.fprs[0], &cpu.fprs()[0], 32 * sizeof(u32)); // Backup the 32 FPRs
	oldThread.cpsr = cpu.getCPSR();   // Backup CPSR
	oldThread.fpscr = cpu.getFPSCR(); // Backup FPSCR

	// Load new context
	std::memcpy(&cpu.regs()[0], &newThread.gprs[0], 16 * sizeof(u32)); // Load 16 GPRs
	std::memcpy(&cpu.fprs()[0], &newThread.fprs[0], 32 * sizeof(u32)); // Load 32 FPRs
	cpu.setCPSR(newThread.cpsr);   // Load CPSR
	cpu.setFPSCR(newThread.fpscr); // Load FPSCR
	cpu.setTLSBase(newThread.tlsBase); // Load CP15 thread-local-storage pointer register

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
	} else if (t.status == ThreadStatus::WaitSleep || t.status == ThreadStatus::WaitSync1 || t.status == ThreadStatus::WaitSyncAll) {
		const u64 elapsedTicks = cpu.getTicks() - t.sleepTick;

		constexpr double ticksPerSec = double(CPU::ticksPerSec);
		constexpr double nsPerTick = ticksPerSec / 1000000000.0;

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

void Kernel::switchToNextThread() {
	std::optional<int> newThreadIndex = getNextThread();
	
	if (!newThreadIndex.has_value()) {
		Helpers::warn("Kernel tried to switch to the next thread but none found. Switching to random thread\n");
		switchThread(rand() % threadCount);
	} else {
		switchThread(newThreadIndex.value());
	}
}

// See if there;s a higher priority, ready thread and switch to that
void Kernel::rescheduleThreads() {
	std::optional<int> newThreadIndex = getNextThread();
	
	if (newThreadIndex.has_value()) {
		threads[currentThreadIndex].status = ThreadStatus::Ready;
		switchThread(newThreadIndex.value());
	}
}

// Internal OS function to spawn a thread
Handle Kernel::makeThread(u32 entrypoint, u32 initialSP, u32 priority, s32 id, u32 arg, ThreadStatus status) {
	if (threadCount >= appResourceLimits.maxThreads) {
		Helpers::panic("Overflowed the number of threads");
	}
	threadIndices.push_back(threadCount);

	Thread& t = threads[threadCount++]; // Reference to thread data
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

	t.cpsr = CPSR::UserMode | (isThumb ? CPSR::Thumb : 0);
	t.fpscr = FPSCR::ThreadDefault;
	// Initial TLS base has already been set in Kernel::Kernel()
	// TODO: Does svcCreateThread zero-set the TLS of the new thread?

	sortThreads();
	return ret;
}

Handle Kernel::makeMutex(bool locked) {
	Handle ret = makeObject(KernelObjectType::Mutex);
	objects[ret].data = new Mutex(locked);

	// If the mutex is initially locked, store the index of the thread that owns it
	if (locked) {
		objects[ret].getData<Mutex>()->ownerThread = currentThreadIndex;
	}

	return ret;
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

	switchToNextThread();
}

// Make a thread sleep for a certain amount of nanoseconds at minimum
void Kernel::sleepThread(s64 ns) {
	if (ns < 0) {
		Helpers::panic("Sleeping a thread for a negative amount of ns");
	} else if (ns == 0) { // Used when we want to force a thread switch
		int curr = currentThreadIndex;
		switchToNextThread(); // Mark thread as ready after switching, to avoid switching to the same thread
		threads[curr].status = ThreadStatus::Ready;
	} else { // If we're sleeping for > 0 ns
		Thread& t = threads[currentThreadIndex];
		t.status = ThreadStatus::WaitSleep;
		t.waitingNanoseconds = ns;
		t.sleepTick = cpu.getTicks();

		switchToNextThread();
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
		regs[0] = SVCResult::BadThreadPriority;
		return;
	}

	regs[0] = SVCResult::Success;
	regs[1] = makeThread(entrypoint, initialSP, priority, id, arg, ThreadStatus::Ready);
}

// void SleepThread(s64 nanoseconds)
void Kernel::svcSleepThread() {
	const s64 ns = s64(u64(regs[0]) | (u64(regs[1]) << 32));
	logSVC("SleepThread(ns = %lld)\n", ns);

	sleepThread(ns);
	regs[0] = SVCResult::Success;
}

void Kernel::getThreadID() {
	Handle handle = regs[1];
	logSVC("GetThreadID(handle = %X)\n", handle);

	if (handle == KernelHandles::CurrentThread) {
		regs[0] = SVCResult::Success;
		regs[1] = currentThreadIndex;
		return;
	}

	const auto thread = getObject(handle, KernelObjectType::Thread);
	if (thread == nullptr) [[unlikely]] {
		regs[0] = SVCResult::BadHandle;
		return;
	}

	regs[0] = SVCResult::Success;
	regs[1] = thread->getData<Thread>()->index;
}

void Kernel::getThreadPriority() {
	const Handle handle = regs[1];
	log("GetThreadPriority (handle = %X)\n", handle);

	if (handle == KernelHandles::CurrentThread) {
		regs[0] = SVCResult::Success;
		regs[1] = threads[currentThreadIndex].priority;
	} else {
		auto object = getObject(handle, KernelObjectType::Thread);
		if (object == nullptr) [[unlikely]] {
			regs[0] = SVCResult::BadHandle;
		} else {
			regs[0] = SVCResult::Success;
			regs[1] = object->getData<Thread>()->priority;
		}
	}
}

void Kernel::setThreadPriority() {
	const Handle handle = regs[0];
	const u32 priority = regs[1];
	log("SetThreadPriority (handle = %X, priority = %X)\n", handle, priority);

	if (priority > 0x3F) {
		regs[0] = SVCResult::BadThreadPriority;
		return;
	}

	if (handle == KernelHandles::CurrentThread) {
		regs[0] = SVCResult::Success;
		threads[currentThreadIndex].priority = priority;
	} else {
		auto object = getObject(handle, KernelObjectType::Thread);
		if (object == nullptr) [[unlikely]] {
			regs[0] = SVCResult::BadHandle;
			return;
		} else {
			regs[0] = SVCResult::Success;
			object->getData<Thread>()->priority = priority;
		}
	}
	sortThreads();
	rescheduleThreads();
}

void Kernel::createMutex() {
	bool locked = regs[1] != 0;
	logSVC("CreateMutex (locked = %s)\n", locked ? "yes" : "no");

	regs[0] = SVCResult::Success;
	regs[1] = makeMutex(locked);
}

void Kernel::releaseMutex() {
	const Handle handle = regs[0];

	logSVC("ReleaseMutex (handle = %x) (STUBBED)\n", handle);
	regs[0] = SVCResult::Success;
}

// Returns whether an object is waitable or not
// The KernelObject type enum is arranged in a specific order in kernel_types.hpp so this
// can simply compile to a fast sub+cmp+set despite looking slow
bool Kernel::isWaitable(const KernelObject* object) {
	auto type = object->type;
	using enum KernelObjectType;

	return type == Event || type == Mutex || type == Port || type == Semaphore || type == Timer || type == Thread;
}

// Returns whether we should wait on a sync object or not
bool Kernel::shouldWaitOnObject(KernelObject* object) {
	switch (object->type) {
		case KernelObjectType::Event: // We should wait on an event only if it has not been signalled
			return !object->getData<Event>()->fired;

		default:
			Helpers::warn("Not sure whether to wait on object (type: %s)", object->getTypeName());
			return true;
	}
}