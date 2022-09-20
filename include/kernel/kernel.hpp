#pragma once
#include <array>
#include <limits>
#include <string>
#include <vector>
#include "kernel_types.hpp"
#include "helpers.hpp"
#include "memory.hpp"
#include "services/service_manager.hpp"

class CPU;

class Kernel {
	std::array<u32, 16>& regs;
	CPU& cpu;
	Memory& mem;

	// The handle number for the next kernel object to be created
	u32 handleCounter;
	std::vector<KernelObject> objects;
	std::vector<Handle> portHandles;

	Handle currentProcess;
	Handle currentThread;
	Handle mainThread;
	Handle srvHandle; // Handle for the special service manager port "srv:"
	u32 arbiterCount;
	u32 threadCount;
	ServiceManager serviceManager;

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

	Handle makeObject(KernelObjectType type) {
		if (handleCounter > KernelHandles::Max) [[unlikely]] {
			Helpers::panic("Hlep we somehow created enough kernel objects to overflow this thing");
		}

		objects.push_back(KernelObject(handleCounter, type));
		printf("Created %s object with handle %d\n", kernelObjectTypeToString(type), handleCounter);
		return handleCounter++;
	}

	Handle makeArbiter();
	Handle makeEvent(ResetType resetType);
	Handle makeProcess();
	Handle makePort(const char* name);
	Handle makeSession(Handle port);
	Handle makeThread(u32 entrypoint, u32 initialSP, u32 priority, u32 id, ThreadStatus status = ThreadStatus::Dormant);

	std::optional<Handle> getPortHandle(const char* name);
	void deleteObjectData(KernelObject& object);

	KernelObject* getProcessFromPID(Handle handle);
	s32 getCurrentResourceValue(const KernelObject* limit, u32 resourceName);
	u32 getMaxForResource(const KernelObject* limit, u32 resourceName);
	u32 getTLSPointer();

	std::string getProcessName(u32 pid);
	const char* resetTypeToString(u32 type);

	// SVC implementations
	void arbitrateAddress();
	void createAddressArbiter();
	void createEvent();
	void createThread();
	void controlMemory();
	void mapMemoryBlock();
	void queryMemory();
	void getResourceLimit();
	void getResourceLimitLimitValues();
	void getResourceLimitCurrentValues();
	void sendSyncRequest();
	void svcCloseHandle();
	void connectToPort();
	void outputDebugString();

public:
	Kernel(CPU& cpu, Memory& mem);
	void serviceSVC(u32 svc);
	void reset();
};