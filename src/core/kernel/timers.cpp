#include <limits>

#include "cpu.hpp"
#include "kernel.hpp"
#include "scheduler.hpp"

HorizonHandle Kernel::makeTimer(ResetType type) {
	Handle ret = makeObject(KernelObjectType::Timer);
	objects[ret].data = new Timer(type);

	if (type == ResetType::Pulse) {
		Helpers::panic("Created pulse timer");
	}

	timerHandles.push_back(ret);
	return ret;
}

void Kernel::pollTimers() {
	u64 currentTick = cpu.getTicks();

	// Find the next timestamp we'll poll KTimers on. To do this, we find the minimum tick one of our timers will fire
	u64 nextTimestamp = std::numeric_limits<u64>::max();
	// Do we have any active timers anymore? If not, then we won't need to schedule a new timer poll event
	bool haveActiveTimers = false;

	for (auto handle : timerHandles) {
		KernelObject* object = getObject(handle, KernelObjectType::Timer);
		if (object != nullptr) {
			Timer* timer = object->getData<Timer>();

			if (timer->running) {
				// If timer has fired, signal it and set the tick it will next time
				if (currentTick >= timer->fireTick) {
					signalTimer(handle, timer);
				}

				// Update our next timer fire timestamp and mark that we should schedule a new event to poll timers
				// We recheck timer->running because signalling a timer stops it if interval == 0
				if (timer->running) {
					nextTimestamp = std::min<u64>(nextTimestamp, timer->fireTick);
					haveActiveTimers = true;
				}
			}
		}
	}

	// If we still have active timers, schedule next poll event
	if (haveActiveTimers) {
		Scheduler& scheduler = cpu.getScheduler();
		scheduler.addEvent(Scheduler::EventType::UpdateTimers, nextTimestamp);
	}
}

void Kernel::cancelTimer(Timer* timer) {
	timer->running = false;
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

	if (timer->interval == 0) {
		cancelTimer(timer);
	} else {
		timer->fireTick = cpu.getTicks() + Scheduler::nsToCycles(timer->interval);
	}
}

void Kernel::svcCreateTimer() {
	const u32 resetType = regs[1];
	if (resetType > 2) {
		Helpers::panic("Invalid reset type for event %d", resetType);
	}

	// Have a warning here until our timers don't suck
	Helpers::warn("Called Kernel::CreateTimer. Timers are currently not updated nor triggered properly!");

	logSVC("CreateTimer (resetType = %s)\n", resetTypeToString(resetType));
	regs[0] = Result::Success;
	regs[1] = makeTimer(static_cast<ResetType>(resetType));
}

void Kernel::svcSetTimer() {
	Handle handle = regs[0];
	// TODO: Is this actually s64 or u64? 3DBrew says s64, but u64 makes more sense
	const s64 initial = s64(u64(regs[2]) | (u64(regs[3]) << 32));
	const s64 interval = s64(u64(regs[1]) | (u64(regs[4]) << 32));
	logSVC("SetTimer (handle = %X, initial delay = %llX, interval delay = %llX)\n", handle, initial, interval);

	KernelObject* object = getObject(handle, KernelObjectType::Timer);

	if (object == nullptr) {
		Helpers::panic("Tried to set non-existent timer %X\n", handle);
		regs[0] = Result::Kernel::InvalidHandle;
	}

	Timer* timer = object->getData<Timer>();
	cancelTimer(timer);
	timer->interval = interval;
	timer->running = true;
	timer->fireTick = cpu.getTicks() + Scheduler::nsToCycles(initial);
	
	Scheduler& scheduler = cpu.getScheduler();
	// Signal an event to poll timers as soon as possible
	scheduler.removeEvent(Scheduler::EventType::UpdateTimers);
	scheduler.addEvent(Scheduler::EventType::UpdateTimers, cpu.getTicks() + 1);

	// If the initial delay is 0 then instantly signal the timer
	if (initial == 0) {
		signalTimer(handle, timer);
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