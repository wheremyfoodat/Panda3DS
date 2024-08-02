#pragma once

#include <fcntl.h>
#include <linux/kvm.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/mman.h>

#include "helpers.hpp"
#include "kernel.hpp"
#include "memory.hpp"

#define AARCH64_CORE_REG(x) (KVM_REG_ARM64 | KVM_REG_SIZE_U64 | KVM_REG_ARM_CORE | KVM_REG_ARM_CORE_REG(x))

struct MmuTables {
    u32 level1[4096];
    u32 level2SectionTables[256];
};

constexpr u32 hypervisorCodeAddress = 0xD0000000;
constexpr u32 hypervisorDataAddress = 0xE0000000;
constexpr u32 hypervisorCodeSize = hypervisorDataAddress - hypervisorCodeAddress;
constexpr u32 hypervisorDataSize = hypervisorCodeSize;
constexpr u32 mmuTableOffset = hypervisorDataSize - sizeof(MmuTables);
constexpr u32 mmuTableAddress = hypervisorDataAddress + mmuTableOffset;
constexpr u32 exitCodeOffset = 0; // at start of hypervisor data segment
constexpr u32 customEntryOffset = 0x100000; // arbitrary, far enough that the exit code won't ever overlap with this
constexpr u32 guestStateOffset = 0x200000; // also arbitrary, store the guest state here upon exit

struct GuestState
{
    std::array<u32, 16> regs;
    std::array<u32, 32> fprs;
    u32 cpsr;
    u32 fpscr;
	// u32 tlsBase?
    // u64 ticks?
};

struct Environment {
	Environment(Memory& mem, Kernel& kernel) : mem(mem), kernel(kernel) {
		u32 currentMemorySlot = 0;

		kvmDescriptor = open("/dev/kvm", O_RDWR);
		if (kvmDescriptor < 0) {
			Helpers::panic("Failed to open /dev/kvm");
		}

		vmDescriptor = ioctl(kvmDescriptor, KVM_CREATE_VM, 0);
		if (vmDescriptor < 0) {
			Helpers::panic("Failed to create KVM VM");
		}

		if (ioctl(vmDescriptor, KVM_CHECK_EXTENSION, KVM_CAP_ARM_EL1_32BIT) <= 0) {
			Helpers::panic("CPU doesn't support EL1 32-bit mode, KVM won't work on this CPU");
		}

		// TODO: allocate these with mmap instead of malloc
		kvm_userspace_memory_region vramRegionDesc = {
			.slot = currentMemorySlot++,
			.flags = 0,
			.guest_phys_addr = PhysicalAddrs::VRAM,
			.memory_size = PhysicalAddrs::VRAMSize,
			.userspace_addr = (uint64_t)mem.getVRAM()};
		if (ioctl(vmDescriptor, KVM_SET_USER_MEMORY_REGION, &vramRegionDesc) < 0) {
			Helpers::panic("Failed to set VRAM memory region");
		}

		kvm_userspace_memory_region dspRegionDesc = {
			.slot = currentMemorySlot++,
			.flags = 0,
			.guest_phys_addr = PhysicalAddrs::DSPMem,
			.memory_size = PhysicalAddrs::DSPMemSize,
			.userspace_addr = (uint64_t)mem.getDSPMem()};
		if (ioctl(vmDescriptor, KVM_SET_USER_MEMORY_REGION, &dspRegionDesc) < 0) {
			Helpers::panic("Failed to set DSP memory region");
		}

		kvm_userspace_memory_region fcramRegionDesc = {
			.slot = currentMemorySlot++,
			.flags = 0,
			.guest_phys_addr = PhysicalAddrs::FCRAM,
			.memory_size = PhysicalAddrs::FCRAMSize * 2,
			.userspace_addr = (uint64_t)mem.getFCRAM()};
		if (ioctl(vmDescriptor, KVM_SET_USER_MEMORY_REGION, &fcramRegionDesc) < 0) {
			Helpers::panic("Failed to set FCRAM memory region");
		}

		hypervisorCodeRegion = mmap(NULL, hypervisorCodeSize, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
		if (hypervisorCodeRegion == MAP_FAILED) {
			Helpers::panic("Failed to allocate memory for hypervisor I/O");
		}

		kvm_userspace_memory_region hypervisorCodeRegionDesc = {
			.slot = currentMemorySlot++,
			.flags = KVM_MEM_READONLY, // We want writes here to cause VM exits
			.guest_phys_addr = hypervisorCodeAddress,
			.memory_size = hypervisorCodeSize,
			.userspace_addr = (uint64_t)hypervisorCodeRegion
		};

		if (ioctl(vmDescriptor, KVM_SET_USER_MEMORY_REGION, &hypervisorCodeRegionDesc) < 0) {
			Helpers::panic("Failed to set up hypervisor IO memory region\n");
			return;
		}

		hypervisorDataRegion = mmap(NULL, hypervisorDataSize, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (hypervisorDataRegion == MAP_FAILED) {
			Helpers::panic("Failed to allocate memory for hypervisor code");
		}

		kvm_userspace_memory_region hypervisorDataRegionDesc = {
			.slot = currentMemorySlot++,
			.flags = 0,
			.guest_phys_addr = hypervisorDataAddress,
			.memory_size = hypervisorDataSize,
			.userspace_addr = (uint64_t)hypervisorDataRegion
		};

		if (ioctl(vmDescriptor, KVM_SET_USER_MEMORY_REGION, &hypervisorDataRegionDesc) < 0) {
			Helpers::panic("Failed to set up hypervisor code memory region\n");
			return;
		}

		cpuDescriptor = ioctl(vmDescriptor, KVM_CREATE_VCPU, 0);
		if (cpuDescriptor < 0) {
			Helpers::panic("Failed to create VCPU");
		}

		int mmapSize = ioctl(kvmDescriptor, KVM_GET_VCPU_MMAP_SIZE, 0);
		if (mmapSize < 0) {
			Helpers::panic("Failed to get KVM shared memory size");
		}

		runInfo = (kvm_run*)mmap(nullptr, mmapSize, PROT_READ | PROT_WRITE, MAP_SHARED, cpuDescriptor, 0);
		if (runInfo == MAP_FAILED) {
			Helpers::panic("Failed to map KVM shared memory");
		}

		kvm_vcpu_init initParams;
		if (ioctl(vmDescriptor, KVM_ARM_PREFERRED_TARGET, &initParams) < 0) {
			Helpers::panic("Failed to fetch initialization parameters for vCPU");
		}
		initParams.features[0] |= 1 << KVM_ARM_VCPU_EL1_32BIT;
		initParams.features[0] |= 1 << KVM_ARM_VCPU_PSCI_0_2;

		if (ioctl(cpuDescriptor, KVM_ARM_VCPU_INIT, initParams) < 0) {
			Helpers::panic("Failed to initialize vCPU");
		}

		kvm_reg_list tempRegList;
		tempRegList.n = 0;
		ioctl(cpuDescriptor, KVM_GET_REG_LIST, &tempRegList);

		regList = (kvm_reg_list*)malloc(sizeof(kvm_reg_list) + tempRegList.n * sizeof(u64));
		regList->n = tempRegList.n;
		if (ioctl(cpuDescriptor, KVM_GET_REG_LIST, regList) < 0) {
			Helpers::panic("Failed to get register list");
		}
	}

	void setPC(u32 pc) {
		u64 val = (u64)pc;
		kvm_one_reg reg;

		reg.id = AARCH64_CORE_REG(regs.pc);
		reg.addr = (u64)&val;

		if (ioctl(cpuDescriptor, KVM_SET_ONE_REG, &reg) < 0) [[unlikely]] {
			printf("SetPC failed\n");
		}
	}

	void run() {
		if (ioctl(cpuDescriptor, KVM_RUN, 0) < 0) {
			Helpers::panic("Failed to run vCPU");
		} else {
			printf("KVM run succeeded\n");
		}
	}

	void mapHypervisorCode(const std::vector<u8>& code, u32 offset)
	{
		if (code.size() > hypervisorCodeSize) {
			Helpers::panic("Launch code is too big");
		}
		memcpy((void*)((uintptr_t)hypervisorCodeRegion + offset), code.data(), code.size());
	}

	Memory& mem;
	Kernel& kernel;
	kvm_run* runInfo = nullptr;
	kvm_reg_list* regList = nullptr;
	void* hypervisorCodeRegion;
	void* hypervisorDataRegion;
	int kvmDescriptor = -1;
	int vmDescriptor = -1;
	int cpuDescriptor = -1;
};

class CPU {
	Memory& mem;
	Environment env;
	GuestState state;

  public:
	static constexpr u64 ticksPerSec = 268111856;

	CPU(Memory& mem, Kernel& kernel);
	void reset() {}

	void setReg(int index, u32 value) {}
	u32 getReg(int index) {return 0;}

	std::span<u32, 16> regs() { return state.regs; }
	std::span<u32, 32> fprs() { return state.fprs; }

	void setCPSR(u32 value) { state.cpsr = value; }
	u32 getCPSR() { return state.cpsr; }
	void setFPSCR(u32 value) { state.fpscr = value; }
	u32 getFPSCR() { return state.fpscr; }
	void setTLSBase(u32 value) {}

	u64 getTicks() {return 0;}
	u64& getTicksRef() {static u64 dummy; return dummy;}

	void clearCache() {}

	void runFrame() {}

	// TODO: remove
	void romLoaded();
};

#undef AARCH64_CORE_REG