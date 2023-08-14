#include "kernel.hpp"
#include "cpu.hpp"

Handle Kernel::makeTimer(ResetType type) {
	Handle ret = makeObject(KernelObjectType::Timer);
	objects[ret].data = new Timer(type);

	if (type == ResetType::Pulse) {
		Helpers::panic("Created pulse timer");
	}

	timerHandles.push_back(ret);
	return ret;
}

void Kernel::updateTimer(Handle handle, Timer* timer) {
	if (timer->running) {
		const u64 currentTicks = cpu.getTicks();
		u64 elapsedTicks = currentTicks - timer->startTick;

		constexpr double ticksPerSec = double(CPU::ticksPerSec);
		constexpr double nsPerTick = ticksPerSec / 1000000000.0;
		const s64 elapsedNs = s64(double(elapsedTicks) * nsPerTick);

		// Timer has fired
		if (elapsedNs >= timer->currentDelay) {
			timer->startTick = currentTicks;
			timer->currentDelay = timer->interval;
			signalTimer(handle, timer);
		}
	}
}

void Kernel::cancelTimer(Timer* timer) {
	timer->running = false;
	// TODO: When we have a scheduler this should properly cancel timer events in the scheduler
}

void Kernel::signalTimer(Handle timerHandle, Timer* timer) {
	timer->fired = true;
	requireReschedule();

	// Check if there's any thread waiting on this event
	if (timer->waitlist != 0) {
		wakeupAllThreads(timer->waitlist, timerHandle);
		timer->waitlist = 0;  // No threads waiting;

		switch (timer->resetType) {
			case ResetType::OneShot: timer->fired = false; break;
			case ResetType::Sticky: break;
			case ResetType::Pulse: Helpers::panic("Signalled pulsing timer"); break;
		}
	}
}

void Kernel::svcCreateTimer() {
	const u32 resetType = regs[1];
	if (resetType > 2) {
		Helpers::panic("Invalid reset type for event %d", resetType);
	}

	// Have a warning here until our timers don't suck
	Helpers::warn("Called Kernel::CreateTimer");

	logSVC("CreateTimer (resetType = %s)\n", resetTypeToString(resetType));
	regs[0] = Result::Success;
	regs[1] = makeTimer(static_cast<ResetType>(resetType));
}

void Kernel::svcSetTimer() {
	Handle handle = regs[0];
	// TODO: Is this actually s64 or u64? 3DBrew says s64, but u64 makes more sense
	const s64 initial = s64(u64(regs[1]) | (u64(regs[2]) << 32));
	const s64 interval = s64(u64(regs[3]) | (u64(regs[4]) << 32));
	logSVC("SetTimer (handle = %X, initial delay = %llX, interval delay = %llX)\n", handle, initial, interval);

	KernelObject* object = getObject(handle, KernelObjectType::Timer);

	if (object == nullptr) {
		Helpers::panic("Tried to set non-existent timer %X\n", handle);
		regs[0] = Result::Kernel::InvalidHandle;
	}

	Timer* timer = object->getData<Timer>();
	cancelTimer(timer);
	timer->currentDelay = initial;
	timer->interval = interval;
	timer->running = true;
	timer->startTick = cpu.getTicks();

	// If the initial delay is 0 then instantly signal the timer
	if (initial == 0) {
		signalTimer(handle, timer);
	} else {
		// This should schedule an event in the scheduler when we have one
	}
	
	regs[0] = Result::Success;
}

void Kernel::svcClearTimer() {
	Handle handle = regs[0];
	logSVC("ClearTimer (handle = %X)\n", handle);
	KernelObject* object = getObject(handle, KernelObjectType::Timer);

	if (object == nullptr) {
		Helpers::panic("Tried to clear non-existent timer %X\n", handle);
		regs[0] = Result::Kernel::InvalidHandle;
	} else {
		object->getData<Timer>()->fired = false;
		regs[0] = Result::Success;
	}
}

void Kernel::svcCancelTimer() {
	Handle handle = regs[0];
	logSVC("CancelTimer (handle = %X)\n", handle);
	KernelObject* object = getObject(handle, KernelObjectType::Timer);

	if (object == nullptr) {
		Helpers::panic("Tried to cancel non-existent timer %X\n", handle);
		regs[0] = Result::Kernel::InvalidHandle;
	} else {
		cancelTimer(object->getData<Timer>());
		regs[0] = Result::Success;
	}
}