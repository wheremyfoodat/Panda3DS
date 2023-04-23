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
	logSVC("CreateAddressArbiter\n");
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

	logSVC("ArbitrateAddress(Handle = %X, address = %08X, type = %s, value = %d, ns = %lld)\n", handle, address,
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
	// This needs to put the error code in r0 before we change threads
	regs[0] = SVCResult::Success;

	switch (static_cast<ArbitrationType>(type)) {
		// Puts this thread to sleep if word < value until another thread arbitrates the address using SIGNAL
		case ArbitrationType::WaitIfLess: {
			s32 word = static_cast<s32>(mem.read32(address)); // Yes this is meant to be signed 
			if (word < value) {
				sleepThreadOnArbiter(address);
			}
			break;
		}

		// Puts this thread to sleep if word < value until another thread arbitrates the address using SIGNAL
		// If the thread is put to sleep, the arbiter address is decremented
		case ArbitrationType::DecrementAndWaitIfLess: {
			s32 word = static_cast<s32>(mem.read32(address)); // Yes this is meant to be signed 
			if (word < value) {
				mem.write32(address, word - 1);
				sleepThreadOnArbiter(address);
			}
			break;
		}

		case ArbitrationType::Signal:
			signalArbiter(address, value);
			break;

		default:
			Helpers::panic("ArbitrateAddress: Unimplemented type %s", arbitrationTypeToString(type));
	}
}

// Signal up to "threadCount" threads waiting on the arbiter indicated by "waitingAddress"
void Kernel::signalArbiter(u32 waitingAddress, s32 threadCount) {
	if (threadCount == 0) [[unlikely]] return;
	s32 count = 0; // Number of threads we've woken up

	// Wake threads with the highest priority threads being woken up first
	for (auto index : threadIndices) {
		Thread& t = threads[index];
		if (t.status == ThreadStatus::WaitArbiter && t.waitingAddress == waitingAddress) {
			t.status = ThreadStatus::Ready;
			count += 1;

			// Check if we've reached the max number of. If count < 0 then all threads are released.
			if (count == threadCount && threadCount > 0) break;
		}
	}

	if (count != 0) {
		rescheduleThreads();
	}
}