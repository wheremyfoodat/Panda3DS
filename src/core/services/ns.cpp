#include "services/ns.hpp"
#include "ipc.hpp"

namespace NSCommands {
    enum : u32 {
        LaunchTitle = 0x000200C0,
    };
}

void NSService::reset() {}

void NSService::handleSyncRequest(u32 messagePointer, Type type) {
    const u32 command = mem.read32(messagePointer);

    // ns:s commands
    switch (command) {
        case NSCommands::LaunchTitle: launchTitle(messagePointer); break;

        default: Helpers::panic("NS service requested. Command: %08X\n", command);
    }
}

void NSService::launchTitle(u32 messagePointer) {
    const u64 titleID = mem.read64(messagePointer + 4);
    const u32 launchFlags = mem.read32(messagePointer + 12);
    log("NS::LaunchTitle (title ID = %llX, launch flags = %X) (stubbed)\n", titleID, launchFlags);

    mem.write32(messagePointer, IPC::responseHeader(0x2, 2, 0));
    mem.write32(messagePointer + 4, Result::Success);
    mem.write32(messagePointer + 8, 0); // Process ID
}