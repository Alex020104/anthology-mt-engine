// The engine already imports Vista+ thread-pool APIs. Keep the same minimum
// locally, without changing SDK levels in legacy headers/PCH consumers.
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "FramePacer.h"

// The SDK hides this flag behind the Win10 target level; requesting it is still
// safe on our Vista+ minimum because creation failure takes the regular fallback.
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace
{
	FramePacerClock::Tick Now()
	{
		LARGE_INTEGER value;
		QueryPerformanceCounter(&value);
		return FramePacerClock::Tick(value.QuadPart);
	}
}

FramePacerWait::~FramePacerWait() { Close(); }

void FramePacerWait::Close()
{
	if (timer_) CloseHandle(timer_);
	timer_ = nullptr;
	initialized_ = high_resolution_ = false;
	oversleep_ = 0;
}

void FramePacerWait::Initialize()
{
	if (initialized_) return;
	initialized_ = true;
	timer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
		TIMER_MODIFY_STATE | SYNCHRONIZE);
	high_resolution_ = timer_ != nullptr;
	if (!timer_) timer_ = CreateWaitableTimerW(nullptr, FALSE, nullptr);
}

FramePacerWait::Sample FramePacerWait::Until(Tick deadline, Tick frequency)
{
	Sample sample;
	Tick now = Now();
	if (!deadline || now >= deadline || !frequency) return sample;
	Initialize();
	now = Now(); // Lazy timer creation also consumes the current frame's remainder.
	oversleep_ -= oversleep_ / 16;
	// This adapts only the OS wake-up guard, never the requested frame rate.
	// Most of the remainder is a kernel wait; the short final guard uses PAUSE.
	const Tick floor = frequency / 5000; // 200 us bootstrap, not a work budget.
	Tick guard = oversleep_ + frequency / 20000;
	if (guard < floor) guard = floor;
	while (deadline > now && deadline - now > guard)
	{
		const Tick wake = deadline - guard;
		const Tick remaining = wake - now;
		const Tick start = now;
		bool waited = false;
		if (timer_)
		{
			LARGE_INTEGER due;
			due.QuadPart = -LONGLONG(double(remaining) * 10000000.0 / double(frequency));
			if (!due.QuadPart) due.QuadPart = -1;
			if (SetWaitableTimer(timer_, &due, 0, nullptr, nullptr, FALSE))
				waited = WaitForSingleObject(timer_, INFINITE) == WAIT_OBJECT_0;
		}
		if (!waited)
		{
			// Timer creation/arming can fail. Sleep only whole milliseconds and
			// retain the same final-clock check rather than spinning an entire frame.
			const DWORD ms = DWORD(double(remaining) * 1000.0 / double(frequency));
			if (!ms) break;
			Sleep(ms);
		}
		now = Now();
		sample.slept += now - start;
		const Tick late = now > wake ? now - wake : 0;
		if (late > oversleep_) oversleep_ = late;
		// An OS suspension must not turn later normal waits into long spins.
		const Tick maximum = frequency / (high_resolution_ ? 1000 : 50);
		if (oversleep_ > maximum) oversleep_ = maximum;
		guard = oversleep_ + frequency / 20000;
		if (guard < floor) guard = floor;
	}
	const Tick spinStart = now;
	while (now < deadline)
	{
		YieldProcessor();
		now = Now();
	}
	sample.spun = now - spinStart;
	return sample;
}
