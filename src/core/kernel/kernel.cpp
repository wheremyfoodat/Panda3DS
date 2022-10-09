#include <cassert>
#include "kernel.hpp"
#include "kernel_types.hpp"
#include "cpu.hpp"

Kernel::Kernel(CPU& cpu, Memory& mem, GPU& gpu)
	: cpu(cpu), regs(cpu.regs()), mem(mem), handleCounter(0), serviceManager(regs, mem, gpu, currentProcess, *this) {
	objects.reserve(512); // Make room for a few objects to avoid further memory allocs later
	portHandles.reserve(32);

	for (int i = 0; i < threads.size(); i++) {
		threads[i].index = i;
		threads[i].tlsBase = VirtualAddrs::TLSBase + i * VirtualAddrs::TLSSize;
		threads[i].status = ThreadStatus::Dead;
	}

	setVersion(1, 69);
}

void Kernel::serviceSVC(u32 svc) {
	switch (svc) {
		case 0x01: controlMemory(); break;
		case 0x02: queryMemory(); break;
		case 0x08: createThread(); break;
		case 0x14: releaseMutex(); break;
		case 0x17: createEvent(); break;
		case 0x19: clearEvent(); break;
		case 0x1F: mapMemoryBlock(); break;
		case 0x21: createAddressArbiter(); break;
		case 0x22: arbitrateAddress(); break;
		case 0x23: svcCloseHandle(); break;
		case 0x24: waitSynchronization1(); break;
		case 0x25: waitSynchronizationN(); break;
		case 0x27: duplicateHandle(); break;
		case 0x28: getSystemTick(); break;
		case 0x2B: getProcessInfo(); break;
		case 0x2D: connectToPort(); break;
		case 0x32: sendSyncRequest(); break;
		case 0x35: getProcessID(); break;
		case 0x37: getThreadID(); break;
		case 0x38: getResourceLimit(); break;
		case 0x39: getResourceLimitLimitValues(); break;
		case 0x3A: getResourceLimitCurrentValues(); break;
		case 0x3D: outputDebugString(); break;
		default: Helpers::panic("Unimplemented svc: %X @ %08X", svc, regs[15]); break;
	}
}

void Kernel::setVersion(u8 major, u8 minor) {
	u16 descriptor = (u16(major) << 8) | u16(minor);

	kernelVersion = descriptor;
	mem.kernelVersion = descriptor; // The memory objects needs a copy because you can read the kernel ver from config mem
}

Handle Kernel::makeProcess(u32 id) {
	const Handle processHandle = makeObject(KernelObjectType::Process);
	const Handle resourceLimitHandle = makeObject(KernelObjectType::ResourceLimit);

	// Allocate data
	objects[processHandle].data = new Process(id);
	const auto processData = objects[processHandle].getData<Process>();

	// Link resource limit object with its parent process
	objects[resourceLimitHandle].data = &processData->limits;
	processData->limits.handle = resourceLimitHandle;
	return processHandle;
}

// Get a pointer to the process indicated by handle, taking into account that 0xFFFF8001 always refers to the current process
// Returns nullptr if the handle does not correspond to a process
KernelObject* Kernel::getProcessFromPID(Handle handle) {
	if (handle == KernelHandles::CurrentProcess) [[likely]] {
		return getObject(currentProcess, KernelObjectType::Process);
	} else {
		return getObject(handle, KernelObjectType::Process);
	}
}

void Kernel::deleteObjectData(KernelObject& object) {
	using enum KernelObjectType;

	// Resource limit and thread objects do not allocate heap data, so we don't delete anything
	if (object.data == nullptr || object.type == ResourceLimit || object.type == Thread) {
		return;
	}

	delete object.data;
}

void Kernel::reset() {
	handleCounter = 0;
	arbiterCount = 0;
	threadCount = 0;

	for (auto& object : objects) {
		deleteObjectData(object);
	}
	objects.clear();
	portHandles.clear();
	serviceManager.reset();

	// Allocate handle #0 to a dummy object and make a main process object
	makeObject(KernelObjectType::Dummy);
	currentProcess = makeProcess(1); // Use ID = 1 for main process

	// Make main thread object. We do not have to set the entrypoint and SP for it as the ROM loader does.
	// Main thread seems to have a priority of 0x30. TODO: This creates a dummy context for thread 0,
	// which is thankfully not used. Maybe we should prevent this
	mainThread = makeThread(0, VirtualAddrs::StackTop, 0x30, -2, 0, ThreadStatus::Running);
	currentThreadIndex = 0;

	// Create some of the OS ports
	srvHandle = makePort("srv:"); // Service manager port
	errorPortHandle = makePort("err:f"); // Error display port
}

// Get pointer to thread-local storage
u32 Kernel::getTLSPointer() {
	return VirtualAddrs::TLSBase + currentThreadIndex * VirtualAddrs::TLSSize;
}

// Result CloseHandle(Handle handle)
void Kernel::svcCloseHandle() {
	logSVC("CloseHandle(handle = %d) (Unimplemented)\n", regs[0]);
	regs[0] = SVCResult::Success;
}

// u64 GetSystemTick()
void Kernel::getSystemTick() {
	logSVC("GetSystemTick()\n");

	u64 ticks = cpu.getTicks();
	regs[0] = u32(ticks);
	regs[1] = u32(ticks >> 32);
}

// Result OutputDebugString(const char* str, s32 size)
// TODO: Does this actually write an error code in r0 and is the above signature correct?
void Kernel::outputDebugString() {
	const u32 pointer = regs[0];
	const u32 size = regs[1];

	std::string message = mem.readString(pointer, size);
	logDebugString("[OutputDebugString] %s\n", message.c_str());
	regs[0] = SVCResult::Success;
}

void Kernel::getProcessID() {
	const auto pid = regs[1];
	const auto process = getProcessFromPID(pid);
	logSVC("GetProcessID(process: %s)\n", getProcessName(pid).c_str());

	if (process == nullptr) [[unlikely]] {
		regs[0] = SVCResult::BadHandle;
		return;
	}

	regs[0] = SVCResult::Success;
	regs[1] = process->getData<Process>()->id;
}

// Result GetProcessInfo(s64* out, Handle process, ProcessInfoType type)
void Kernel::getProcessInfo() {
	const auto pid = regs[1];
	const auto type = regs[2];
	const auto process = getProcessFromPID(pid);
	logSVC("GetProcessInfo(process: %s, type = %d)\n", getProcessName(pid).c_str(), type);

	if (process == nullptr) [[unlikely]] {
		regs[0] = SVCResult::BadHandle;
		return;
	}

	switch (type) {
		case 20: // Returns 0x20000000 - <linear memory base vaddr for process>
			regs[1] = PhysicalAddrs::FCRAM - mem.getLinearHeapVaddr();
			regs[2] = 0;
			break;

		default:
			Helpers::panic("GetProcessInfo: unimplemented type %d", type);
	}

	regs[0] = SVCResult::Success;
}

// Result GetThreadId(u32* threadId, Handle thread)
void Kernel::duplicateHandle() {
	Handle original = regs[1];
	logSVC("DuplicateHandle(handle = %X)\n", original);

	if (original == KernelHandles::CurrentThread) {
		regs[0] = SVCResult::Success;
		Handle ret = makeObject(KernelObjectType::Thread);
		objects[ret].data = &threads[currentThreadIndex];

		regs[1] = ret;
	} else {
		Helpers::panic("DuplicateHandle: unimplemented handle type");
	}
}

std::string Kernel::getProcessName(u32 pid) {
	if (pid == KernelHandles::CurrentProcess) {
		return "current";
	} else {
		Helpers::panic("Attempted to name non-current process");
	}
}