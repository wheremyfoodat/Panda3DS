#include "kernel.hpp"
#include "resource_limits.hpp"

Handle Kernel::makeArbiter() {
	if (arbiterCount >= appResourceLimits.maxAddressArbiters) {
		Helpers::panic("Overflowed the number of address arbiters");
	}
	arbiterCount++;

	Handle ret = makeObject(KernelObjectType::AddressArbiter);
	objects[ret].data = new AddressArbiter();
	return ret;
}

// Result CreateAddressArbiter(Handle* arbiter)
void Kernel::createAddressArbiter() {
	regs[0] = SVCResult::Success;
	regs[1] = makeArbiter();
}

// Result ArbitrateAddress(Handle arbiter, u32 addr, ArbitrationType type, s32 value, s64 nanoseconds)
void Kernel::arbitrateAddress() {
	const Handle handle = regs[0];
	const u32 address = regs[1];
	const u32 type = regs[2];
	const u32 value = regs[3];
	const s64 ns = s64(u64(regs[4]) | (u64(regs[5]) << 32));

	printf("ArbitrateAddress(Handle = %X, address = %08X, type = %d, value = %d, ns = %lld)\n", handle, address, type,
		value, ns);

	const auto arbiter = getObject(handle, KernelObjectType::AddressArbiter);
	if (arbiter == nullptr) [[unlikely]] {
		regs[0] = SVCResult::BadHandle;
		return;
	}

	Helpers::panic("My balls\n");
	regs[0] = SVCResult::Success;
}