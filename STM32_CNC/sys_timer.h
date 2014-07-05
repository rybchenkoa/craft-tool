#pragma once
#include "stm32f10x.h"
#include "common.h"

class SysTimer
{
	public:
		void init()
		{
			RCC->APB2ENR |= RCC_APB2ENR_TIM17EN; //подключаем
			TIM17->PSC = APB_FREQ/1000000-1;      //выставляем частоту тиков
			TIM17->CR1 |= TIM_CR1_CEN;           //запускаем
		}
		
		inline int get()
		{
			return TIM17->CNT;
		}
		
		void delay_ms(int ms)
		{
			int endTime = ms*1000 + get();
			while(endTime - get() > 0);
		}
		
		void delay_mks(int mks)
		{
			int endTime = mks + get();
			while(endTime - get() > 0);
		}
		
		inline int get_ms(int ms)
		{
			return ms*1000 + get();
		}
		
		inline int get_mks(int mks)
		{
			return mks + get();
		}
		
		inline bool check(int endTime)
		{
			return endTime - get() > 0;
		}
};

SysTimer timer;
