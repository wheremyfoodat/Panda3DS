#pragma once
#include <array>
#include <cstring>
#include "fs/archive_base.hpp"
#include "handles.hpp"
#include "helpers.hpp"
#include "result/result.hpp"

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
    u64 waitlist; // A bitfield where each bit symbolizes if the thread with thread with the corresponding index is waiting on the event
    ResetType resetType = ResetType::OneShot;
    bool fired = false;

    Event(ResetType resetType) : resetType(resetType), waitlist(0) {}
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
    WaitSync1,   // Waiting for the single object in the wait list to be ready
    WaitSyncAny, // Wait for one object of the many that might be in the wait list to be ready
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

    // A list of threads waiting for this thread to terminate. Yes, threads are sync objects too.
    u64 threadsWaitingForTermination;
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
    u64 waitlist;           // Refer to the getWaitlist function below for documentation
    Handle ownerThread = 0; // Index of the thread that holds the mutex if it's locked
    Handle handle; // Handle of the mutex itself
    u32 lockCount; // Number of times this mutex has been locked by its daddy. 0 = not locked
    bool locked;

    Mutex(bool lock, Handle handle) : locked(lock), waitlist(0), lockCount(lock ? 1 : 0), handle(handle) {}
};

struct Semaphore {
    u64 waitlist; // Refer to the getWaitlist function below for documentation
    s32 availableCount;
    s32 maximumCount;

    Semaphore(s32 initialCount, s32 maximumCount) : availableCount(initialCount), maximumCount(maximumCount), waitlist(0) {}
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

    const char* getTypeName() const {
        return kernelObjectTypeToString(type);
    }

    // Retrieves a reference to the waitlist for a specified object
    // We return a reference because this function is only called in the kernel threading internals
    // We want the kernel to be able to easily manage waitlists, by reading/parsing them or setting/clearing bits.
    // As we mention in the definition of the "Event" struct, the format for wailists is very simple and made to be efficient.
    // Each bit corresponds to a thread index and denotes whether the corresponding thread is waiting on this object
    // For example if bit 0 of the wait list is set, then the thread with index 0 is waiting on our object
    u64& getWaitlist() {
        // This code is actually kinda trash but eh good enough
        switch (type) {
            case KernelObjectType::Event: return getData<Event>()->waitlist;
            case KernelObjectType::Mutex: return getData<Mutex>()->waitlist;
            case KernelObjectType::Semaphore: return getData<Mutex>()->waitlist;
            case KernelObjectType::Thread: return getData<Thread>()->threadsWaitingForTermination;
            // This should be unreachable once we fully implement sync objects
            default: [[unlikely]]
                Helpers::panic("Called GetWaitList on kernel object without a waitlist (Type: %s)", getTypeName());
        }
    }
};