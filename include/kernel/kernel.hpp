#pragma once
#include <array>
#include <cassert>
#include <limits>
#include <span>
#include <string>
#include <vector>

#include "config.hpp"
#include "helpers.hpp"
#include "kernel_types.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "resource_limits.hpp"
#include "services/service_manager.hpp"

class CPU;

class Kernel {
	std::span<u32, 16> regs;
	CPU& cpu;
	Memory& mem;

	// The handle number for the next kernel object to be created
	u32 handleCounter;
	// A list of our OS threads, the max number of which depends on the resource limit (hardcoded 32 per process on retail it seems).
	// We have an extra thread for when no thread is capable of running. This thread is called the "idle thread" in our code
	// This thread is set up in setupIdleThread and just yields in a loop to see if any other thread has woken up
	std::array<Thread, appResourceLimits.maxThreads + 1> threads;
	static constexpr int idleThreadIndex = appResourceLimits.maxThreads;
	// Our waitlist system uses a bitfield of 64 bits to show which threads are waiting on an object.
	// That means we can have a maximum of 63 threads + 1 idle thread. This assert should never trigger because the max thread # is 32
	// But we have it here for safety purposes
	static_assert(appResourceLimits.maxThreads <= 63, "The waitlist system is built on the premise that <= 63 threads max can be active");

	std::vector<KernelObject> objects;
	std::vector<Handle> portHandles;
	std::vector<Handle> mutexHandles;

	// Thread indices, sorted by priority
	std::vector<int> threadIndices;

	Handle currentProcess;
	Handle mainThread;
	int currentThreadIndex;
	Handle srvHandle; // Handle for the special service manager port "srv:"
	Handle errorPortHandle; // Handle for the err:f port used for displaying errors

	u32 arbiterCount;
	u32 threadCount; // How many threads in our thread pool have been used as of now (Up to 32)
	u32 aliveThreadCount; // How many of these threads are actually alive?
	ServiceManager serviceManager;

	// Top 8 bits are the major version, bottom 8 are the minor version
	u16 kernelVersion = 0;

	// Shows whether a reschedule will be need
	bool needReschedule = false;

	Handle makeArbiter();
	Handle makeProcess(u32 id);
	Handle makePort(const char* name);
	Handle makeSession(Handle port);
	Handle makeThread(u32 entrypoint, u32 initialSP, u32 priority, ProcessorID id, u32 arg,ThreadStatus status = ThreadStatus::Dormant);
	Handle makeMemoryBlock(u32 addr, u32 size, u32 myPermission, u32 otherPermission);

public:
	Handle makeEvent(ResetType resetType); // Needs to be public to be accessible to the APT/HID services
	Handle makeMutex(bool locked = false); // Needs to be public to be accessible to the APT/DSP services
	Handle makeSemaphore(u32 initialCount, u32 maximumCount); // Needs to be public to be accessible to the service manager port
	Handle makeTimer(ResetType resetType);

	// Signals an event, returns true on success or false if the event does not exist
	bool signalEvent(Handle e);
private:
	void signalArbiter(u32 waitingAddress, s32 threadCount);
	void sleepThread(s64 ns);
	void sleepThreadOnArbiter(u32 waitingAddress);
	void switchThread(int newThreadIndex);
	void sortThreads();
	std::optional<int> getNextThread();
	void rescheduleThreads();
	bool canThreadRun(const Thread& t);
	bool shouldWaitOnObject(KernelObject* object);
	void releaseMutex(Mutex* moo);

	// Wake up the thread with the highest priority out of all threads in the waitlist
	// Returns the index of the woken up thread
	// Do not call this function with an empty waitlist!!!
	int wakeupOneThread(u64 waitlist, Handle handle);
	void wakeupAllThreads(u64 waitlist, Handle handle);

	std::optional<Handle> getPortHandle(const char* name);
	void deleteObjectData(KernelObject& object);

	KernelObject* getProcessFromPID(Handle handle);
	s32 getCurrentResourceValue(const KernelObject* limit, u32 resourceName);
	u32 getMaxForResource(const KernelObject* limit, u32 resourceName);
	u32 getTLSPointer();
	void setupIdleThread();

	void acquireSyncObject(KernelObject* object, const Thread& thread);
	bool isWaitable(const KernelObject* object);

	// Functions for the err:f port
	void handleErrorSyncRequest(u32 messagePointer);
	void throwError(u32 messagePointer);

	std::string getProcessName(u32 pid);
	const char* resetTypeToString(u32 type);

	MAKE_LOG_FUNCTION(log, kernelLogger)
	MAKE_LOG_FUNCTION(logSVC, svcLogger)
	MAKE_LOG_FUNCTION(logThread, threadLogger)
	MAKE_LOG_FUNCTION(logError, errorLogger)
	MAKE_LOG_FUNCTION(logFileIO, fileIOLogger)
	MAKE_LOG_FUNCTION_USER(logDebugString, debugStringLogger)

	// SVC implementations
	void arbitrateAddress();
	void createAddressArbiter();
	void createMemoryBlock();
	void createThread();
	void controlMemory();
	void duplicateHandle();
	void exitThread();
	void mapMemoryBlock();
	void queryMemory();
	void getCurrentProcessorNumber();
	void getProcessID();
	void getProcessInfo();
	void getResourceLimit();
	void getResourceLimitLimitValues();
	void getResourceLimitCurrentValues();
	void getSystemInfo();
	void getSystemTick();
	void getThreadID();
	void getThreadPriority();
	void sendSyncRequest();
	void setThreadPriority();
	void svcCancelTimer();
	void svcClearEvent();
	void svcClearTimer();
	void svcCloseHandle();
	void svcCreateEvent();
	void svcCreateMutex();
	void svcCreateSemaphore();
	void svcCreateTimer();
	void svcReleaseMutex();
	void svcReleaseSemaphore();
	void svcSignalEvent();
	void svcSetTimer();
	void svcSleepThread();
	void connectToPort();
	void outputDebugString();
	void waitSynchronization1();
	void waitSynchronizationN();

	// File operations
	void handleFileOperation(u32 messagePointer, Handle file);
	void closeFile(u32 messagePointer, Handle file);
	void flushFile(u32 messagePointer, Handle file);
	void readFile(u32 messagePointer, Handle file);
	void writeFile(u32 messagePointer, Handle file);
	void getFileSize(u32 messagePointer, Handle file);
	void openLinkFile(u32 messagePointer, Handle file);
	void setFileSize(u32 messagePointer, Handle file);
	void setFilePriority(u32 messagePointer, Handle file);

	// Directory operations
	void handleDirectoryOperation(u32 messagePointer, Handle directory);
	void closeDirectory(u32 messagePointer, Handle directory);
	void readDirectory(u32 messagePointer, Handle directory);

public:
	Kernel(CPU& cpu, Memory& mem, GPU& gpu, const EmulatorConfig& config);
	void initializeFS() { return serviceManager.initializeFS(); }
	void setVersion(u8 major, u8 minor);
	void serviceSVC(u32 svc);
	void reset();

	void requireReschedule() { needReschedule = true; }

	void evalReschedule() {
		if (needReschedule) {
			needReschedule = false;
			rescheduleThreads();
		}
	}

	Handle makeObject(KernelObjectType type) {
		if (handleCounter > KernelHandles::Max) [[unlikely]] {
			Helpers::panic("Hlep we somehow created enough kernel objects to overflow this thing");
		}

		objects.push_back(KernelObject(handleCounter, type));
		log("Created %s object with handle %d\n", kernelObjectTypeToString(type), handleCounter);
		return handleCounter++;
	}

	std::vector<KernelObject>& getObjects() {
		return objects;
	}

	// Get pointer to the object with the specified handle
	KernelObject* getObject(Handle handle) {
		// Accessing an object that has not been created
		if (handle >= objects.size()) [[unlikely]] {
			return nullptr;
		}

		return &objects[handle];
	}

	// Get pointer to the object with the specified handle and type
	KernelObject* getObject(Handle handle, KernelObjectType type) {
		if (handle >= objects.size() || objects[handle].type != type) [[unlikely]] {
			return nullptr;
		}

		return &objects[handle];
	}

	ServiceManager& getServiceManager() { return serviceManager; }

	void sendGPUInterrupt(GPUInterrupt type) { serviceManager.sendGPUInterrupt(type); }
	void signalDSPEvents() { serviceManager.signalDSPEvents(); }
};