#pragma once

#include <cstdint>

#pragma pack(push, 8)

// Owner-thread presentation clock. No simulation dt, GPU waits or worker ownership.
// Kept independent of the OS so deadline arithmetic is tested with a synthetic clock.
class FramePacerClock
{
public:
	using Tick = std::uint64_t;
	struct Sample { Tick interval = 0, late = 0; bool rebased = false; };

	static unsigned EffectiveRate(int requested, bool menuOrPaused, unsigned monitorRate)
	{
		const unsigned rate = requested > 0 ? unsigned(requested) : 0;
		if (monitorRate <= 1) monitorRate = 60; // Unknown/default display frequency.
		return menuOrPaused && (!rate || rate > monitorRate) ? monitorRate : rate;
	}
	void Reset() { *this = FramePacerClock(); }
	bool Configure(Tick now, Tick frequency, unsigned rate, unsigned context)
	{
		if (frequency != frequency_ || rate != rate_ || context != context_ ||
			(last_ && now < last_) || (deadline_ && now > deadline_ && now - deadline_ > frequency))
		{
			Reset();
			frequency_ = frequency;
			rate_ = rate;
			context_ = context;
			if (rate_) { period_ = NextPeriod(); deadline_ = now + period_; }
			return true;
		}
		return false;
	}
	Sample Presented(Tick now)
	{
		Sample sample;
		if (last_ && now >= last_) sample.interval = now - last_;
		last_ = now;
		if (deadline_ && now > deadline_) sample.late = now - deadline_;
		// Correct tiny wake-up error against the existing phase. Genuine missed
		// deadlines start a new cadence: no burst of short catch-up frames.
		Tick tolerance = period_ / 100;
		if (tolerance > frequency_ / 10000) tolerance = frequency_ / 10000;
		if (sample.late > tolerance)
		{
			deadline_ = now;
			remainder_ = 0;
			sample.rebased = true;
		}
		period_ = rate_ ? NextPeriod() : 0;
		deadline_ = period_ ? deadline_ + period_ : 0;
		return sample;
	}
	Tick Deadline() const { return deadline_; }
	unsigned Rate() const { return rate_; }
	Tick Period() const { return period_; }

private:
	Tick NextPeriod()
	{
		Tick period = frequency_ / rate_;
		remainder_ += frequency_ % rate_;
		period += remainder_ / rate_;
		remainder_ %= rate_;
		return period;
	}
	Tick frequency_ = 0, deadline_ = 0, last_ = 0, remainder_ = 0, period_ = 0;
	unsigned rate_ = 0, context_ = 0;
};

// A single reusable native timer, not a clock or thread. QPC measures time;
// this object sleeps without busy-polling for the entire frame remainder.
// No message dispatch while render/Lua ownership are live. Regular-timer fallback
// is retained for Windows versions without the high-resolution timer flag.
class FramePacerWait
{
public:
	using Tick = FramePacerClock::Tick;
	struct Sample { Tick slept = 0, spun = 0; };
	FramePacerWait() = default;
	~FramePacerWait();
	FramePacerWait(const FramePacerWait&) = delete;
	FramePacerWait& operator=(const FramePacerWait&) = delete;
	Sample Until(Tick deadline, Tick frequency);
	void Close();
	bool HighResolution() const { return high_resolution_; }

private:
	void Initialize();
	void* timer_ = nullptr;
	bool initialized_ = false, high_resolution_ = false;
	Tick oversleep_ = 0;
};

#pragma pack(pop)
