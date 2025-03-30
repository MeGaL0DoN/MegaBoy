#pragma once

class RTC
{
public:
	virtual void enableFastForward(int speedFactor) = 0;
	virtual void disableFastForward() = 0;
};