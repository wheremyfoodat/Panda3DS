#include "kernel.hpp"

void Kernel::serviceSVC(u32 svc) {
	switch (svc) {
		case 0x21: createAddressArbiter(); break;
		default: Helpers::panic("Unimplemented svc: %x", svc); break;
	}
}

void Kernel::reset() {

}

// Result CreateAddressArbiter(Handle* arbiter)
// in: r0 -> Pointer to Handle object
// out: r0 -> result
void Kernel::createAddressArbiter() {
	printf("Stubbed call to CreateAddressArbiter. Handle address: %08X\n", regs[0]);
	regs[0] = SVCResult::Success;
}