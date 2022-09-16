#include "resource_limits.hpp"
#include "kernel.hpp"

// Result GetResourceLimit(Handle* resourceLimit, Handle process)
// out: r0 -> result, r1 -> handle
void Kernel::getResourceLimit() {
	const auto handlePointer = regs[0];
	const auto pid = regs[1];
	const auto process = getProcessFromPID(pid);

	if (process == nullptr) [[unlikely]] {
		regs[0] = SVCResult::BadHandle;
		return;
	}

	const auto processData = static_cast<ProcessData*>(process->data);

	printf("GetResourceLimit (handle pointer = %08X, process: %s)\n", handlePointer, getProcessName(pid).c_str());
	mem.write32(handlePointer, processData->limits.handle);
	regs[0] = SVCResult::Success;
	// Is the handle meant to be output through both r1 and memory? Libctru breaks otherwise.
	regs[1] = processData->limits.handle;
}

// Result GetResourceLimitLimitValues(s64* values, Handle resourceLimit, LimitableResource* names, s32 nameCount)
void Kernel::getResourceLimitLimitValues() {
	u32 values = regs[0]; // Pointer to values (The resource limits get output here)
	const Handle resourceLimit = regs[1];
	u32 names = regs[2];  // Pointer to resources that we should return
	u32 count = regs[3];  // Number of resources

	const KernelObject* limit = getObject(resourceLimit, KernelObjectType::ResourceLimit);
	if (limit == nullptr) [[unlikely]] {
		regs[0] = SVCResult::BadHandle;
		return;
	}

	printf("GetResourceLimitLimitValues(values = %08X, handle = %X, names = %08X, count = %d)\n", values, resourceLimit, names, count);
	// printf("[Warning] We do not currently support any resource maximum aside from the application ones");
	while (count != 0) {
		const u32 name = mem.read32(names);
		u32 max = getMaxForResource(limit, name);
		mem.write64(values, u64(max));

		// Increment pointers and decrement count
		values += sizeof(u64);
		names += sizeof(u32);
		count--;
	}

	regs[0] = SVCResult::Success;
}

// Result GetResourceLimitCurrentValues(s64* values, Handle resourceLimit, LimitableResource* names, s32 nameCount)
void Kernel::getResourceLimitCurrentValues() {
	u32 values = regs[0]; // Pointer to values (The resource limits get output here)
	const Handle resourceLimit = regs[1];
	u32 names = regs[2];  // Pointer to resources that we should return
	u32 count = regs[3];  // Number of resources

	const KernelObject* limit = getObject(resourceLimit, KernelObjectType::ResourceLimit);
	if (limit == nullptr) [[unlikely]] {
		regs[0] = SVCResult::BadHandle;
		return;
	}

	printf("GetResourceLimitCurrentValues(values = %08X, handle = %X, names = %08X, count = %d)\n", values, resourceLimit, names, count);
	while (count != 0) {
		const u32 name = mem.read32(names);
		// TODO: Unsure if this is supposed to be s32 or u32. Shouldn't matter as the kernel can't create so many resources
		s32 value = getCurrentResourceValue(limit, name);
		mem.write64(values, u64(value));

		// Increment pointers and decrement count
		values += sizeof(u64);
		names += sizeof(u32);
		count--;
	}

	regs[0] = SVCResult::Success;
}

s32 Kernel::getCurrentResourceValue(const KernelObject* limit, u32 resourceName) {
	const auto data = static_cast<ResourceLimits*>(limit->data);
	switch (resourceName) {
		case ResourceType::Commit: return mem.usedUserMemory;
		default: Helpers::panic("Attempted to get current value of unknown kernel resource: %d\n", resourceName);
	}
}

u32 Kernel::getMaxForResource(const KernelObject* limit, u32 resourceName) {
	switch (resourceName) {
		case ResourceType::Commit: return appResourceLimits.maxCommit;
		default: Helpers::panic("Attempted to get the max of unknown kernel resource: %d\n", resourceName);
	}
}