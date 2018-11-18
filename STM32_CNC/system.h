//ǁ
extern "C" {
void init_fault_irq(void)
{
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
	NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
	int priority = NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0);
	NVIC_SetPriority(MemoryManagement_IRQn, priority);
	NVIC_SetPriority(BusFault_IRQn, priority);
	NVIC_SetPriority(UsageFault_IRQn, priority);
}

void init_system_clock()
{
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
}

// подаем питание на используемую периферию
void connect_peripherals()
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN
                | RCC_AHB1ENR_GPIOCEN
                | RCC_AHB1ENR_GPIODEN
				| RCC_AHB1ENR_GPIOFEN
                | RCC_AHB1ENR_GPIOGEN
                | RCC_AHB1ENR_DMA1EN
                | RCC_AHB1ENR_DMA2EN
				| RCC_AHB1ENR_CRCEN;

	RCC->APB2ENR |= RCC_APB2ENR_TIM8EN
				| RCC_APB2ENR_ADC1EN
				| RCC_APB2ENR_USART1EN;
								
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN
				| RCC_APB1ENR_TIM3EN
                | RCC_APB1ENR_TIM4EN
				| RCC_APB1ENR_TIM5EN
                | RCC_APB1ENR_TIM6EN
                | RCC_APB1ENR_TIM7EN;
}

/*подключено на плате, на всякий случай выключаем, чтобы не спалить порты
	a0, e3, e4 - кнопки
	a9, a10 uart
	a11, a12 usb
	f9, f10 led
	b2 boot1 через резистор и через джампер
	b3, b4 flash
*/
void init_default_gpio()
{
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
}




void NMI_Handler()
{
	RCC->CIR |= RCC_CIR_CSSC; //если сломалось тактирование от внешнего кварца
	log_console("err: external quarz has broken, use internal source %d\n", 0);//сообщим об этом и будем работать от внутреннего
}

void HardFault_Handler()
{
	while (1)
	{
	}
}

void MemManage_Handler()
{
	while (1)
	{
	}
}

void BusFault_Handler()
{
	while (1)
	{
	}
}

void UsageFault_Handler()
{
	while (1)
	{
	}
}

//без поддержки конструкторов статических объектов
//void __libc_init_array (){}
//без поддержки исключений
//void __aeabi_unwind_cpp_pr0 (void){}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line)
{ 
}
#endif
}
