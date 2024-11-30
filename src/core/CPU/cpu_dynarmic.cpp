#ifdef CPU_DYNARMIC
#include "cpu_dynarmic.hpp"

#include "arm_defs.hpp"
#include "emulator.hpp"

CPU::CPU(Memory& mem, Kernel& kernel, Emulator& emu) : mem(mem), emu(emu), scheduler(emu.getScheduler()), env(mem, kernel, emu.getScheduler()) {
	cp15 = std::make_shared<CP15>();
	mem.setCPUTicks(getTicksRef());

	Dynarmic::A32::UserConfig config;
	config.arch_version = Dynarmic::A32::ArchVersion::v6K;
	config.callbacks = &env;
	config.coprocessors[15] = cp15;
	config.define_unpredictable_behaviour = true;
	config.global_monitor = &exclusiveMonitor;
	config.processor_id = 0;

	if (mem.isFastmemEnabled()) {
		config.fastmem_pointer = u64(mem.getFastmemArenaBase());
	} else {
		config.fastmem_pointer = std::nullopt;
	}

	jit = std::make_unique<Dynarmic::A32::Jit>(config);
}

void CPU::reset() {
	setCPSR(CPSR::UserMode);
	setFPSCR(FPSCR::MainThreadDefault);

	cp15->reset();
	cp15->setTLSBase(VirtualAddrs::TLSBase);  // Set cp15 TLS pointer to the main thread's thread-local storage
	jit->Reset();
	jit->ClearCache();
	jit->Regs().fill(0);
	jit->ExtRegs().fill(0);
}

void CPU::runFrame() {
	emu.frameDone = false;

	while (!emu.frameDone) {
		// Run CPU until the next scheduler event
		env.ticksLeft = scheduler.nextTimestamp - scheduler.currentTimestamp;

	execute:
		const auto exitReason = jit->Run();

		// Handle any scheduler events that need handling.
		emu.pollScheduler();

		if (static_cast<u32>(exitReason) != 0) [[unlikely]] {
			// Cache invalidation needs to exit the JIT so it returns a CacheInvalidation HaltReason. In our case, we just go back to executing
			// The goto might be terrible but it does guarantee that this does not recursively call run and crash, instead getting optimized to a jump
			if (Dynarmic::Has(exitReason, Dynarmic::HaltReason::CacheInvalidation)) {
				goto execute;
			} else {
				Helpers::panic("Exit reason: %d\nPC: %08X", static_cast<u32>(exitReason), getReg(15));
			}
		}
	}
}

#endif  // CPU_DYNARMIC