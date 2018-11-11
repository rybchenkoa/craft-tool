#pragma once
#include "stm32f4xx.h"
#include "common.h"

class SysTimer
{
	public:
		int highCounter; //таймер тикает на уменьшение, поэтому здесь лежит накопленное время + макс. значение
		void init()
		{
          /*
          highCounter = SysTick_LOAD_RELOAD_Msk + 1;
          SysTick_Config(SysTick_LOAD_RELOAD_Msk);
          */
          /*
          CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	      DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
          */
          RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;
          TIM5->PSC = 83; //APB1*2 - 84 МГц
          TIM5->CNT = 0;
          TIM5->ARR = -1;
          TIM5->EGR |= TIM_EGR_UG; //обновляем значение в PSC
          TIM5->CR1 |= TIM_CR1_CEN;
		}
		
		inline int get()
		{
            //return DWT->CYCCNT;
            //return (highCounter - SysTick->VAL); //mks*168
			return TIM5->CNT; //mks
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
/*
extern "C" void SysTick_Handler(void)
{
	timer.highCounter += SysTick_LOAD_RELOAD_Msk + 1;
}
*/
