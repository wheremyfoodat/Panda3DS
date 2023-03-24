#pragma once
#include <array>
#include <cstring>
#include "fs/archive_base.hpp"
#include "handles.hpp"
#include "helpers.hpp"

namespace SVCResult {
	enum : u32 {
		Success = 0,
		Failure = 0xFFFFFFFF,
        ObjectNotFound = 0xD88007FA,

        // Different calls return a different value for these ones
        InvalidEnumValue = 0xD8E007ED,
        InvalidEnumValueAlt = 0xD8E093ED,
        BadHandle = 0xD8E007F7,
        BadHandleAlt = 0xD9001BF7,

        InvalidCombination = 0xE0E01BEE, // Used for invalid memory permission combinations
        UnalignedAddr = 0xE0E01BF1,
        UnalignedSize = 0xE0E01BF2,

        BadThreadPriority = 0xE0E01BFD,
        PortNameTooLong = 0xE0E0181E,
	};
}

enum class KernelObjectType : u8 {
    AddressArbiter, Archive, Directory, File, MemoryBlock, Process, ResourceLimit, Session, Dummy,
    // Bundle waitable objects together in the enum to let the compiler optimize certain checks better
    Event, Mutex, Port, Semaphore, Timer, Thread
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

struct AddressArbiter {};

struct ResourceLimits {
    Handle handle;

    s32 currentCommit = 0;
};

struct Process {
    // Resource limits for this process
    ResourceLimits limits;
    // Process ID
    u32 id;

    Process(u32 id) : id(id) {}
};

struct Event {
    ResetType resetType = ResetType::OneShot;
    bool fired = false;

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

enum class ThreadStatus {
    Running,     // Currently running
    Ready,       // Ready to run
    WaitArbiter, // Waiting on an address arbiter
    WaitSleep,   // Waiting due to a SleepThread SVC
    WaitSync1,   // Waiting for AT LEAST one sync object in its wait list to be ready
    WaitSyncAll, // Waiting for ALL sync objects in its wait list to be ready
    WaitIPC,     // Waiting for the reply from an IPC request
    Dormant,     // Created but not yet made ready
    Dead         // Run to completion, or forcefully terminated
};

struct Thread {
    u32 initialSP;  // Initial r13 value
    u32 entrypoint; // Initial r15 value
    u32 priority;
    u32 arg;
    s32 processorID;
    ThreadStatus status;
    Handle handle;  // OS handle for this thread
    int index; // Index of the thread. 0 for the first thread, 1 for the second, and so on
 
    // The waiting address for threads that are waiting on an AddressArbiter
    u32 waitingAddress;

    // The nanoseconds until a thread wakes up from being asleep or from timing out while waiting on an arbiter
    u64 waitingNanoseconds;
    // The tick this thread went to sleep on
    u64 sleepTick;
    // For WaitSynchronization(N): A vector of objects this thread is waiting for
    std::vector<Handle> waitList;
    // For WaitSynchronizationN: Shows whether the object should wait for all objects in the wait list or just one
    bool waitAll;
    // For WaitSynchronizationN: The "out" pointer
    u32 outPointer;

    // Thread context used for switching between threads
    std::array<u32, 16> gprs;
    std::array<u32, 32> fprs; // Stored as u32 because dynarmic does it
    u32 cpsr;
    u32 fpscr;
    u32 tlsBase; // Base pointer for thread-local storage
};

static const char* kernelObjectTypeToString(KernelObjectType t) {
    switch (t) {
        case KernelObjectType::AddressArbiter: return "address arbiter";
        case KernelObjectType::Archive: return "archive";
        case KernelObjectType::Directory: return "directory";
        case KernelObjectType::Event: return "event";
        case KernelObjectType::File: return "file";
        case KernelObjectType::MemoryBlock: return "memory block";
        case KernelObjectType::Port: return "port";
        case KernelObjectType::Process: return "process";
        case KernelObjectType::ResourceLimit: return "resource limit";
        case KernelObjectType::Session: return "session";
        case KernelObjectType::Mutex: return "mutex";
        case KernelObjectType::Semaphore: return "semaphore";
        case KernelObjectType::Thread: return "thread";
        case KernelObjectType::Dummy: return "dummy";
        default: return "unknown";
    }
}

struct Mutex {
    Handle ownerThread = 0; // Index of the thread that holds the mutex if it's locked
    bool locked;

    Mutex(bool lock = false) : locked(lock) {}
};

struct Semaphore {
    u32 initialCount;
    u32 maximumCount;

    Semaphore(u32 initialCount, u32 maximumCount) : initialCount(initialCount), maximumCount(maximumCount) {}
};

struct MemoryBlock {
    u32 addr = 0;
    u32 size = 0;
    u32 myPermission = 0;
    u32 otherPermission = 0;
    bool mapped = false;

    MemoryBlock(u32 addr, u32 size, u32 myPerm, u32 otherPerm) : addr(addr), size(size), myPermission(myPerm), otherPermission(otherPerm),
        mapped(false) {}
};

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

    const char* getTypeName() {
        return kernelObjectTypeToString(type);
    }
};