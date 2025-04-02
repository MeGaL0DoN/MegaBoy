#pragma once
#include <chrono>

class RTC
{
public:
	virtual void enableFastForward(int speedFactor) {};
	virtual void disableFastForward() {};

protected:
	inline uint64_t getUnixTime() const
	{
		return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}
};