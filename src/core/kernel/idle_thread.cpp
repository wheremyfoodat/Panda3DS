#include <cstring>
#include "arm_defs.hpp"
#include "kernel.hpp"

/*
	This file sets up an idle thread that's meant to run when no other OS thread can run.
	It simply idles and constantly yields to check if there's any other thread that can run
	The code for our idle thread looks like this

idle_thread_main:
	mov r0, #4096            @ Loop counter
	
	.loop:
	nop; nop; nop; nop       @ NOP 4 times to waste some cycles
	subs r0, #1              @ Decrement counter by 1, go back to looping if loop counter != 0
	bne .loop

	// Sleep for 0 seconds with the SleepThread SVC, which just yields execution
	mov r0, #0
	mov r1, #0
	svc SleepThread

	b idle_thread_main
*/

static constexpr u8 idleThreadCode[] = {
	0x01, 0x0A, 0xA0, 0xE3, // mov r0, #4096
	0x00, 0xF0, 0x20, 0xE3, 0x00, 0xF0, 0x20, 0xE3, 0x00, 0xF0, 0x20, 0xE3, 0x00, 0xF0, 0x20, 0xE3, // nop (4 times)
	0x01, 0x00, 0x50, 0xE2,  // subs r0, #1
	0xF9, 0xFF, 0xFF, 0x1A,  // bne loop
	0x00, 0x00, 0xA0, 0xE3,  // mov r0, #0
	0x00, 0x10, 0xA0, 0xE3,  // mov r1, #0
	0x0A, 0x00, 0x00, 0xEF,  // svc SleepThread
	0xF4, 0xFF, 0xFF, 0xEA   // b idle_thread_main
};

// Set up an idle thread to run when no thread is able to run
void Kernel::setupIdleThread() {
	Thread& t = threads[idleThreadIndex];
	constexpr u32 codeAddress = 0xBFC00000;

	// Reserve some memory for the idle thread's code. We map this memory to vaddr BFC00000 which is not userland-accessible
	// We only allocate 4KB (1 page) because our idle code is pretty small
	const u32 fcramIndex = mem.allocateSysMemory(Memory::pageSize);
	auto vaddr = mem.allocateMemory(codeAddress, fcramIndex, Memory::pageSize, true, true, false, true, false, true);
	if (!vaddr.has_value() || vaddr.value() != codeAddress) {
		Helpers::panic("Failed to setup idle thread");
	}
	
	// Copy idle thread code to the allocated FCRAM
	std::memcpy(&mem.getFCRAM()[fcramIndex], idleThreadCode, sizeof(idleThreadCode));

	t.entrypoint = codeAddress;
	t.tlsBase = 0;
	t.gprs[13] = 0; // Set SP & LR to 0 just in case. The idle thread should never access memory, but let's be safe
	t.gprs[14] = 0;
	t.gprs[15] = codeAddress;
	t.cpsr = CPSR::UserMode;
	t.fpscr = FPSCR::ThreadDefault;

	// Our idle thread should have as low of a priority as possible, because, well, it's an idle thread.
	// We handle this by giving it a priority of 0xff, which is lower than is actually allowed for user threads
	// (High priority value = low priority)
	t.priority = 0xff;
	t.status = ThreadStatus::Ready;

	// Add idle thread to the list of thread indices
	threadIndices.push_back(idleThreadIndex);
	sortThreads();
}