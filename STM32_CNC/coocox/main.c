//🐖
#include "stm32f4xx_hal_conf.h"

static void Init_IRQ(void)
{
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_CRC);

	NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

	HAL_NVIC_SetPriority(MemoryManagement_IRQn, 0, 0);
	HAL_NVIC_SetPriority(BusFault_IRQn, 0, 0);
	HAL_NVIC_SetPriority(UsageFault_IRQn, 0, 0);
	HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

void Init_SystemClock()
{
	//LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
	//LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);

	LL_UTILS_ClkInitTypeDef initClk;
	initClk.AHBCLKDivider = LL_RCC_SYSCLK_DIV_1;
	initClk.APB1CLKDivider = LL_RCC_APB1_DIV_4;
	initClk.APB2CLKDivider = LL_RCC_APB2_DIV_2;

	LL_UTILS_PLLInitTypeDef initPll;
	initPll.PLLM = LL_RCC_PLLM_DIV_4;
	initPll.PLLN = 168;
	initPll.PLLP = LL_RCC_PLLP_DIV_2;

	LL_PLL_ConfigSystemClock_HSE(168000000, LL_UTILS_HSEBYPASS_OFF, &initPll, &initClk);

	LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);
	LL_InitTick(168000000, 1000);
}

static void Init_GPIO()
{
	//LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);
	//LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOH);
	//LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
}

int main()
{
	Init_IRQ();
	Init_SystemClock();
	Init_GPIO();
	Init_CRC();
	while (1)
	{
	}
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line)
{ 
}
#endif
