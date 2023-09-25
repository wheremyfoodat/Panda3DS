#include "services/ns.hpp"
#include "ipc.hpp"

namespace NSCommands {
    enum : u32 {
        a,
    };
}

void NSService::reset() {}

void NSService::handleSyncRequest(u32 messagePointer, Type type) {
    const u32 command = mem.read32(messagePointer);

    // ns:s commands
    switch (command) {
        default: Helpers::panic("NS service requested. Command: %08X\n", command);
    }
}