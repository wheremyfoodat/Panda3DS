#include <cassert>
#include "kernel.hpp"
#include "kernel_types.hpp"

void Kernel::serviceSVC(u32 svc) {
	switch (svc) {
		case 0x21: createAddressArbiter(); break;
		case 0x38: getResourceLimit(); break;
		case 0x39: getResourceLimitLimitValues(); break;
		default: Helpers::panic("Unimplemented svc: %x @ %08X", svc, regs[15]); break;
	}
}

Handle Kernel::makeProcess() {
	const Handle processHandle = makeObject(KernelObjectType::Process);
	const Handle resourceLimitHandle = makeObject(KernelObjectType::ResourceLimit);

	// Allocate data
	objects[processHandle].data = new ProcessData();
	const auto processData = static_cast<ProcessData*>(objects[processHandle].data);

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

void Kernel::reset() {
	handleCounter = 0;

	for (auto& object : objects) {
		if (object.data != nullptr) {
			delete object.data;
		}
	}
	objects.clear();

	// Make a main process object
	currentProcess = makeProcess();
}

// Result CreateAddressArbiter(Handle* arbiter)
// out: r0 -> result
void Kernel::createAddressArbiter() {
	printf("Stubbed call to CreateAddressArbiter. Handle address: %08X\n", regs[0]);
	regs[0] = SVCResult::Success;
}

// Result GetResourceLimit(Handle* resourceLimit, Handle process)
// out: r0 -> result
void Kernel::getResourceLimit() {
	const auto handlePointer = regs[0];
	const auto pid = regs[1];
	const auto process = getProcessFromPID(pid);

	if (process == nullptr) [[unlikely]] {
		regs[0] = SVCResult::BadHandle;
		return;
	}

	const auto processData = static_cast<ProcessData*>(process->data);

	printf("GetResourceLimit (Handle pointer = %08X, process: %s)\n", handlePointer, getProcessName(pid).c_str());
	mem.write32(handlePointer, processData->limits.handle);
	regs[0] = SVCResult::Success;
	// Is the handle meant to be output through both r1 and memory? Libctru breaks otherwise.
	regs[1] = processData->limits.handle;
}

void Kernel::getResourceLimitLimitValues() {
	Helpers::panic("Trying to getResourceLimitLimitValues from resourceLimit with handle %08X\n", regs[1]);
}

std::string Kernel::getProcessName(u32 pid) {
	if (pid == KernelHandles::CurrentProcess) {
		return "current";
	} else {
		Helpers::panic("Attempted to name non-current process");
	}
}