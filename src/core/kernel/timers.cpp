#include "kernel.hpp"

Handle Kernel::makeTimer(ResetType type) {
	Handle ret = makeObject(KernelObjectType::Timer);
	objects[ret].data = new Timer(type);

	return ret;
}

void Kernel::svcCreateTimer() {
	const u32 resetType = regs[1];
	if (resetType > 2) {
		Helpers::panic("Invalid reset type for event %d", resetType);
	}

	logSVC("CreateTimer (resetType = %s)\n", resetTypeToString(resetType));
	regs[0] = Result::Success;
	regs[1] = makeTimer(static_cast<ResetType>(resetType));
}

void Kernel::svcSetTimer() { Helpers::panic("Kernel::SetTimer"); }
void Kernel::svcClearTimer() { Helpers::panic("Kernel::ClearTimer"); }
void Kernel::svcCancelTimer() { Helpers::panic("Kernel::CancelTimer"); }