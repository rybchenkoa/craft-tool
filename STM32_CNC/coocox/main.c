//ǁ
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

	LL_PLL_ConfigSystemClock_HSE(8000000, LL_UTILS_HSEBYPASS_OFF, &initPll, &initClk);
	LL_FLASH_SetLatency(LL_FLASH_LATENCY_5);

	//LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);
	//LL_InitTick(168000000, 1000);
}

static void Init_GPIO()
{
	//LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
	//LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
	//LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);
	//LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOD);
	//LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOE);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOF);
	//LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOG);

	LL_GPIO_InitTypeDef gpio;

	//входы
	gpio.Mode = LL_GPIO_MODE_INPUT;
	//кнопки
	gpio.Pin = LL_GPIO_PIN_3 | LL_GPIO_PIN_4;
	gpio.Pull = LL_GPIO_PULL_UP;
	LL_GPIO_Init(GPIOE, &gpio);

	gpio.Pin = LL_GPIO_PIN_0;
	gpio.Pull = LL_GPIO_PULL_DOWN;
	LL_GPIO_Init(GPIOA, &gpio);

	//usb
	gpio.Pin = LL_GPIO_PIN_11 | LL_GPIO_PIN_12;
	gpio.Pull = LL_GPIO_PULL_NO;
	LL_GPIO_Init(GPIOA, &gpio);

	//boot 1, внешний flash SCK, MISO
	gpio.Pin = LL_GPIO_PIN_2 | LL_GPIO_PIN_3 | LL_GPIO_PIN_4;
	LL_GPIO_Init(GPIOB, &gpio);

	//выходы
	gpio.Pull = LL_GPIO_PULL_NO;
	//светодиоды
	gpio.Pin = LL_GPIO_PIN_9 | LL_GPIO_PIN_10;
	gpio.Mode = LL_GPIO_MODE_OUTPUT;
	gpio.Speed = LL_GPIO_SPEED_FREQ_LOW;
	gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	LL_GPIO_Init(GPIOF, &gpio);

	//usart
	gpio.Pin = LL_GPIO_PIN_9|LL_GPIO_PIN_10;
	gpio.Mode = LL_GPIO_MODE_ALTERNATE;
	gpio.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	gpio.Alternate = LL_GPIO_AF_7;
	LL_GPIO_Init(GPIOA, &gpio);
}

int timestamp()
{
	return DWT->CYCCNT;
}

void delay(int time)
{
	int next = timestamp() + time;
	while(next - timestamp() > 0);
}

int main()
{
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
	Init_IRQ();
	Init_SystemClock();
	Init_GPIO();

	while (1)
	{
		LL_GPIO_SetOutputPin(GPIOF, LL_GPIO_PIN_9|LL_GPIO_PIN_10);
		delay(SystemCoreClock/4);
		LL_GPIO_ResetOutputPin(GPIOF, LL_GPIO_PIN_9|LL_GPIO_PIN_10);
		delay(SystemCoreClock/4);
	}
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line)
{ 
}
#endif
