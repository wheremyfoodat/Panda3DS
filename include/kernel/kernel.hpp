#pragma once
#include <array>
#include <limits>
#include <string>
#include <vector>
#include "kernel_types.hpp"
#include "helpers.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "resource_limits.hpp"
#include "services/service_manager.hpp"

class CPU;

class Kernel {
	std::array<u32, 16>& regs;
	CPU& cpu;
	Memory& mem;

	// The handle number for the next kernel object to be created
	u32 handleCounter;
	std::array<Thread, appResourceLimits.maxThreads> threads;
	std::vector<KernelObject> objects;
	std::vector<Handle> portHandles;

	Handle currentProcess;
	Handle mainThread;
	int currentThreadIndex;
	Handle srvHandle; // Handle for the special service manager port "srv:"
	Handle errorPortHandle; // Handle for the err:f port used for displaying errors

	u32 arbiterCount;
	u32 threadCount;
	ServiceManager serviceManager;

	// Top 8 bits are the major version, bottom 8 are the minor version
	u16 kernelVersion = 0;

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

	Handle makeArbiter();
	Handle makeEvent(ResetType resetType);
	Handle makeProcess(u32 id);
	Handle makePort(const char* name);
	Handle makeSession(Handle port);
	Handle makeThread(u32 entrypoint, u32 initialSP, u32 priority, s32 id, u32 arg,ThreadStatus status = ThreadStatus::Dormant);

	void sleepThreadOnArbiter(u32 waitingAddress);
	void switchThread(int newThreadIndex);

	std::optional<Handle> getPortHandle(const char* name);
	void deleteObjectData(KernelObject& object);

	KernelObject* getProcessFromPID(Handle handle);
	s32 getCurrentResourceValue(const KernelObject* limit, u32 resourceName);
	u32 getMaxForResource(const KernelObject* limit, u32 resourceName);
	u32 getTLSPointer();

	// Functions for the err:f port
	void handleErrorSyncRequest(u32 messagePointer);
	void throwError(u32 messagePointer);

	std::string getProcessName(u32 pid);
	const char* resetTypeToString(u32 type);

	MAKE_LOG_FUNCTION(log, kernelLogger)
	MAKE_LOG_FUNCTION(logSVC, svcLogger)
	MAKE_LOG_FUNCTION(logDebugString, debugStringLogger)
	MAKE_LOG_FUNCTION(logError, errorLogger)

	// SVC implementations
	void arbitrateAddress();
	void clearEvent();
	void createAddressArbiter();
	void createEvent();
	void createThread();
	void controlMemory();
	void duplicateHandle();
	void mapMemoryBlock();
	void queryMemory();
	void getProcessID();
	void getProcessInfo();
	void getResourceLimit();
	void getResourceLimitLimitValues();
	void getResourceLimitCurrentValues();
	void getSystemTick();
	void getThreadID();
	void sendSyncRequest();
	void svcCloseHandle();
	void connectToPort();
	void outputDebugString();
	void waitSynchronization1();

	// File operations
	void handleFileOperation(u32 messagePointer, Handle file);
	void readFile(u32 messagePointer, Handle file);

public:
	Kernel(CPU& cpu, Memory& mem, GPU& gpu);
	void setVersion(u8 major, u8 minor);
	void serviceSVC(u32 svc);
	void reset();

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

	void sendGPUInterrupt(GPUInterrupt type) { serviceManager.requestGPUInterrupt(type); }
};