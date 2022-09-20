#include <cstring>
#include "kernel.hpp"
#include "arm_defs.hpp"
// This header needs to be included because I did stupid forward decl hack so the kernel and CPU can both access each other
#include "cpu.hpp"
#include "resource_limits.hpp"

// Switch to another thread
// newThread: Index of the newThread in the thread array (NOT a handle).
void Kernel::switchThread(int newThreadIndex) {
	if (currentThreadIndex == newThreadIndex) { // Bail early if the new thread is actually the old thread
		return;
	}

	auto& oldThread = threads[currentThreadIndex];
	const auto& newThread = threads[newThreadIndex];

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

// Internal OS function to spawn a thread
Handle Kernel::makeThread(u32 entrypoint, u32 initialSP, u32 priority, s32 id, u32 arg, ThreadStatus status) {
	if (threadCount >= appResourceLimits.maxThreads) {
		Helpers::panic("Overflowed the number of threads");
	}
	Thread& t = threads[threadCount++]; // Reference to thread data
	Handle ret = makeObject(KernelObjectType::Thread);
	objects[ret].data = &t;

	const bool isThumb = (entrypoint & 1) != 0; // Whether the thread starts in thumb mode or not

	// Set up initial thread context
	t.gprs.fill(0);
	t.fprs.fill(0);

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

	return ret;
}

// Result CreateThread(s32 priority, ThreadFunc entrypoint, u32 arg, u32 stacktop, s32 threadPriority, s32 processorID)	
void Kernel::createThread() {
	u32 priority = regs[0];
	u32 entrypoint = regs[1];
	u32 arg = regs[2]; // An argument value stored in r0 of the new thread
	u32 initialSP = regs[3] & ~7; // SP is force-aligned to 8 bytes
	s32 id = static_cast<s32>(regs[4]);

	printf("CreateThread(entry = %08X, stacktop = %08X, priority = %X, processor ID = %d)\n", entrypoint,
		initialSP, priority, id);

	if (priority > 0x3F) [[unlikely]] {
		Helpers::panic("Created thread with bad priority value %X", priority);
		regs[0] = SVCResult::BadThreadPriority;
		return;
	}

	regs[0] = SVCResult::Success;
	regs[1] = makeThread(entrypoint, initialSP, priority, id, arg, ThreadStatus::Ready);
}

void Kernel::sleepThreadOnArbiter(u32 waitingAddress) {
	Thread& t = threads[currentThreadIndex];

	t.status = ThreadStatus::WaitArbiter;
	t.waitingAddress = waitingAddress;
	switchThread(1);
}