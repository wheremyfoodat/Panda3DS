#include "kernel.hpp"

void Kernel::svcCreateTimer() { Helpers::panic("Kernel::CreateTimer"); }
void Kernel::svcSetTimer() { Helpers::panic("Kernel::SetTimer"); }
void Kernel::svcClearTimer() { Helpers::panic("Kernel::ClearTimer"); }
void Kernel::svcCancelTimer() { Helpers::panic("Kernel::CancelTimer"); }