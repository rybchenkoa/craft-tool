#pragma once
// таймер, который возвращает микросекунды и такты

#include <hal/cpu_hal.h>

class SysTimer
{
public:
	void init()
	{
	}

	// число тактов процессора
	inline int get_ticks()
	{
		return cpu_hal_get_cycle_count();
	}

	void delay_ticks(int ticks)
	{
		volatile int endTime = get_ticks() + ticks;
		while(endTime - get() > 0);
	}

	inline int get()
	{
		return micros();
	}

	void delay_ms(int ms)
	{
		volatile int endTime = get_ms(ms);
		while(check(endTime));
	}

	void delay_mks(int mks)
	{
		volatile int endTime = get_mks(mks);
		while(check(endTime));
	}

	inline int get_ms(int ms)
	{
		return ms*1000 + get();
	}

	inline int get_mks(int mks)
	{
		return mks + get();
	}

	inline int get_mks(int from, int mks)
	{
		return mks + from;
	}

	inline bool check(int endTime)
	{
		return endTime - get() > 0;
	}
};

SysTimer timer;
