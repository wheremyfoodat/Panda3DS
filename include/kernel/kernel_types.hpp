#pragma once
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
    Event, Port, Process, ResourceLimit, Session, Thread, Dummy
};

enum class ResourceLimitCategory : int {
    Application = 0,
    SystemApplet = 1,
    LibraryApplet = 2,
    Misc = 3
};

enum ResourceTypes {
    PRIORITY = 0,
    COMMIT = 1,
    THREAD = 2,
    EVENT = 3,
    MUTEX = 4,
    SEMAPHORE = 5,
    TIMER = 6,
    SHARED_MEMORY = 7,
    ADDRESS_ARBITER = 8,
    CPU_TIME = 9
};

// Reset types (for use with events and timers)
enum ResetType {
    RESET_ONESHOT = 0, // When the primitive is signaled, it will wake up exactly one thread and will clear itself automatically.
    RESET_STICKY = 1, // When the primitive is signaled, it will wake up all threads and it won't clear itself automatically.
    RESET_PULSE = 2, // Only meaningful for timers: same as ONESHOT but it will periodically signal the timer instead of just once.
};

struct ResourceLimits {
    Handle handle;

    s32 currentCommit = 0;
};

struct ProcessData {
    // Resource limits for this process
    ResourceLimits limits;
};

struct EventData {
    ResetType resetType = RESET_ONESHOT;
};

enum class PortType {
    Generic,
    ServiceManager // For the service manager port "srv:"
};

struct PortData {
    static constexpr u32 maxNameLen = 11;

    char name[maxNameLen + 1] = {};
    bool isPublic = false; // Setting name=NULL creates a private port not accessible from svcConnectToPort.
    PortType type = PortType::Generic;
};

struct SessionData {
    Handle portHandle; // The port this session is subscribed to
};

static const char* kernelObjectTypeToString(KernelObjectType t) {
    switch (t) {
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
};