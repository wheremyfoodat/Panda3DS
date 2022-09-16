#pragma once
#include <array>
#include <limits>
#include <string>
#include <vector>
#include "kernel_types.hpp"
#include "helpers.hpp"
#include "memory.hpp"

class Kernel {
	std::array<u32, 16>& regs;
	Memory& mem;

	// The handle number for the next kernel object to be created
	u32 handleCounter;
	std::vector<KernelObject> objects;

	u32 currentProcess;

	// Get pointer to the object with the specified handle
	KernelObject* getObject(u32 handle) {
		// Accessing an object that has not been created
		if (handle >= objects.size()) [[unlikely]] {
			return nullptr;
		}

		return &objects[handle];
	}

	// Get pointer to the object with the specified handle and type
	KernelObject* getObject(u32 handle, KernelObjectType type) {
		if (handle >= objects.size() || objects[handle].type != type) [[unlikely]] {
			return nullptr;
		}

		return &objects[handle];
	}

	Handle makeObject(KernelObjectType type) {
		if (handleCounter == std::numeric_limits<Handle>::max()) [[unlikely]] {
			Helpers::panic("Hlep we somehow created enough kernel objects to overflow this thing");
		}

		objects.push_back(KernelObject(handleCounter, type));
		printf("Created %s object with handle %d\n", kernelObjectTypeToString(type), handleCounter);
		return handleCounter++;
	}

	Handle makeProcess();
	KernelObject* getProcessFromPID(Handle handle);
	s32 getCurrentResourceValue(const KernelObject* limit, u32 resourceName);
	u32 getMaxForResource(const KernelObject* limit, u32 resourceName);
	std::string getProcessName(u32 pid);

	// SVC implementations
	void controlMemory();
	void createAddressArbiter();
	void getResourceLimit();
	void getResourceLimitLimitValues();
	void getResourceLimitCurrentValues();
	void svcCloseHandle();

public:
	Kernel(std::array<u32, 16>& regs, Memory& mem) : regs(regs), mem(mem), handleCounter(0) {
		objects.reserve(512); // Make room for a few objects to avoid further memory allocs later
	}
	void serviceSVC(u32 svc);
	void reset();
};