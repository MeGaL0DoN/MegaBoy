#pragma once
#include "RTC.h"

class HuC3RTC : public RTC
{
public:
	inline void enableFastForward(int speedFactor) override
	{
	}
	inline void disableFastForward() override
	{
	}

private:
};