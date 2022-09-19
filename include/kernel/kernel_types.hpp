#pragma once
#include <cstring>
#include "handles.hpp"
#include "helpers.hpp"

namespace SVCResult {
	enum : u32 {
		Success = 0,
		Failure = 0xFFFFFFFF,
        ObjectNotFound = 0xD88007FA,
        // Different calls return a different value
        BadHandle = 0xD8E007F7,
        BadHandleAlt = 0xD9001BF7,

        BadThreadPriority = 0xE0E01BFD,
        PortNameTooLong = 0xE0E0181E,
	};
}

enum class KernelObjectType : u8 {
    AddressArbiter, Event, Port, Process, ResourceLimit, Session, Thread, Dummy
};

enum class ResourceLimitCategory : int {
    Application = 0,
    SystemApplet = 1,
    LibraryApplet = 2,
    Misc = 3
};

// Reset types (for use with events and timers)
enum class ResetType {
    OneShot = 0, // When the primitive is signaled, it will wake up exactly one thread and will clear itself automatically.
    Sticky = 1, // When the primitive is signaled, it will wake up all threads and it won't clear itself automatically.
    Pulse = 2, // Only meaningful for timers: same as ONESHOT but it will periodically signal the timer instead of just once.
};

enum class ArbitrationType {
    Signal = 0,
    WaitIfLess = 1,
    DecrementAndWaitIfLess = 2,
    WaitIfLessTimeout = 3,
    DecrementAndWaitIfLessTimeout = 4
};

struct AddressArbiter {

};

struct ResourceLimits {
    Handle handle;

    s32 currentCommit = 0;
};

struct Process {
    // Resource limits for this process
    ResourceLimits limits;
};

struct Event {
    ResetType resetType = ResetType::OneShot;

    Event(ResetType resetType) : resetType(resetType) {}
};

struct Port {
    static constexpr u32 maxNameLen = 11;

    char name[maxNameLen + 1] = {};
    bool isPublic = false; // Setting name=NULL creates a private port not accessible from svcConnectToPort.

    Port(const char* name) {
        // If the name is empty (ie the first char is the null terminator) then the port is private
        isPublic = name[0] != '\0';
        std::strncpy(this->name, name, maxNameLen);
    }
};

struct Session {
    Handle portHandle; // The port this session is subscribed to
    Session(Handle portHandle) : portHandle(portHandle) {}
};

struct Thread {
    u32 initialSP;  // Initial r13 value
    u32 entrypoint; // Initial r15 value
    u32 priority;
    u32 processorID;

    Thread(u32 initialSP, u32 entrypoint, u32 priority, u32 processorID) : initialSP(initialSP), entrypoint(entrypoint),
        priority(priority), processorID(processorID) {}
};

static const char* kernelObjectTypeToString(KernelObjectType t) {
    switch (t) {
        case KernelObjectType::AddressArbiter: return "address arbiter";
        case KernelObjectType::Event: return "event";
        case KernelObjectType::Port: return "port";
        case KernelObjectType::Process: return "process";
        case KernelObjectType::ResourceLimit: return "resource limit";
        case KernelObjectType::Session: return "session";
        case KernelObjectType::Thread: return "thread";
        case KernelObjectType::Dummy: return "dummy";
        default: return "unknown";
    }
}

// Generic kernel object class
struct KernelObject {
    Handle handle = 0; // A u32 the OS will use to identify objects
    void* data = nullptr;
    KernelObjectType type;

    KernelObject(Handle handle, KernelObjectType type) : handle(handle), type(type) {}

    // Our destructor does not free the data in order to avoid it being freed when our std::vector is expanded
    // Thus, the kernel needs to delete it when appropriate
    ~KernelObject() {}

    template <typename T>
    T* getData() {
        return static_cast<T*>(data);
    }
};