#include "kernel.hpp"
#include "resource_limits.hpp"

static const char* arbitrationTypeToString(u32 type) {
	switch (type) {
		case 0: return "Signal";
		case 1: return "Wait if less";
		case 2: return "Decrement and wait if less";
		case 3: return "Wait if less with timeout";
		case 4: return "Decrement and wait if less with timeout";
		default: return "Unknown arbitration type";
	}
}

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
	const s32 value = s32(regs[3]);
	const s64 ns = s64(u64(regs[4]) | (u64(regs[5]) << 32));

	printf("ArbitrateAddress(Handle = %X, address = %08X, type = %s, value = %d, ns = %lld)\n", handle, address,
		arbitrationTypeToString(type), value, ns);

	const auto arbiter = getObject(handle, KernelObjectType::AddressArbiter);
	if (arbiter == nullptr) [[unlikely]] {
		regs[0] = SVCResult::BadHandle;
		return;
	}

	if (address & 3) [[unlikely]] {
		Helpers::panic("ArbitrateAddres:: Unaligned address");
	}

	if (type > 4) [[unlikely]] {
		regs[0] = SVCResult::InvalidEnumValueAlt;
		return;
	}
	// This needs to put the error code in r0 before we change threats
	regs[0] = SVCResult::Success;

	switch (static_cast<ArbitrationType>(type)) {
		// Puts this thread to sleep if word < value until another thread signals the address with the type SIGNAL
		case ArbitrationType::WaitIfLess: {
			s32 word = static_cast<s32>(mem.read32(address)); // Yes this is meant to be signed 
			if (word < value) {
				sleepThreadOnArbiter(address);
			}
			break;
		}

		default:
			Helpers::panic("ArbitrateAddress: Unimplemented type %s", arbitrationTypeToString(type));
	}
}