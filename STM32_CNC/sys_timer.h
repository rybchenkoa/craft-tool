#pragma once
#include "stm32f10x.h"
#include "common.h"

class SysTimer
{
	public:
		int highCounter; //таймер тикает на уменьшение, поэтому здесь лежит накопленное время + макс. значение
		void init()
		{
			highCounter = SysTick_LOAD_RELOAD_Msk + 1;
			SysTick_Config(SysTick_LOAD_RELOAD_Msk);
		}
		
		inline int get()
		{
			return (highCounter - SysTick->VAL); //mks*24
		}
		
		void delay_ms(int ms)
		{
			int endTime = get_ms(ms);
			while(endTime - get() > 0);
		}
		
		void delay_mks(int mks)
		{
			int endTime = get_mks(mks);
			while(endTime - get() > 0);
		}
		
		inline int get_ms(int ms)
		{
			return ms*1000*24 + get();
		}
		
		inline int get_mks(int mks)
		{
			return mks*24 + get();
		}
		
		inline bool check(int endTime)
		{
			return endTime - get() > 0;
		}
};

SysTimer timer;

extern "C" void SysTick_Handler(void)
{
	timer.highCounter += SysTick_LOAD_RELOAD_Msk + 1;
}
