//#define STM32F10X_LD_VL
#include "stm32f10x.h"
#include "common.h"

//разводка пинов для подключения ШИМ
//пины нумеруются так: [0,1,2,3],[4,5,6,7],[8,9,10,11],[12,13,14,15], каждая группа отвечает за свой двигатель

//--------------------------------------------------
void connect_timers()
{
	RCC->APB2ENR |= RCC_APB2ENR_AFIOEN
								| RCC_APB2ENR_IOPAEN
								| RCC_APB2ENR_IOPBEN
								| RCC_APB2ENR_TIM1EN
								| RCC_APB2ENR_TIM15EN
								| RCC_APB2ENR_TIM16EN;   	// включаем тактирование портов и таймеров
								
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN
								| RCC_APB1ENR_TIM3EN;
}
//--------------------------------------------------
void set_PWM_mode(TIM_TypeDef *TIM)
{
	TIM->CR1 |= TIM_CR1_ARPE;   //размер импульса ШИМ меняется в начале нового импульса
	TIM->CCMR1 = TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1PE   //левый шим на всех каналах
							| TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2PE;

	TIM->CCMR2 = TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3PE
							| TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4PE;
}

//--------------------------------------------------
void set_PWM_modes()
{
	set_PWM_mode(TIM1);
	set_PWM_mode(TIM2);
	set_PWM_mode(TIM3);
	
	TIM15->CR1 |= TIM_CR1_ARPE;
	TIM15->CCMR1 = TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1PE
							 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2PE;
	
	TIM16->CR1 |= TIM_CR1_ARPE;
	TIM16->CCMR1 = TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 ;//| TIM_CCMR1_OC1PE;
	
	TIM1->BDTR |= TIM_BDTR_MOE;   //второй вариант - задать значения сигналов в начале периода
	TIM15->BDTR |= TIM_BDTR_MOE;  //через биты OIS1, OIS1N и т.д.
	TIM16->BDTR |= TIM_BDTR_MOE;  //описание в RM0041, стр. 264
}

//--------------------------------------------------
void connect_PWM_channels()
{
	AFIO->MAPR |= AFIO_MAPR_TIM2_REMAP_1; //перекидываем tim2 ch3,ch4 на b10, b11
	
	TIM1->CCER |= TIM_CCER_CC1E
							| TIM_CCER_CC4E
							| TIM_CCER_CC1NE
							| TIM_CCER_CC2NE
							| TIM_CCER_CC3NE; //подключаем все каналы
	
	TIM2->CCER |= TIM_CCER_CC1E
							| TIM_CCER_CC2E
							| TIM_CCER_CC3E
							| TIM_CCER_CC4E;
	
	TIM3->CCER |= TIM_CCER_CC1E
							| TIM_CCER_CC2E
							| TIM_CCER_CC3E
							| TIM_CCER_CC4E;
	
	TIM15->CCER |= TIM_CCER_CC1E
							 | TIM_CCER_CC2E;
	
	TIM16->CCER |= TIM_CCER_CC1E;
}

//--------------------------------------------------
void set_pwm_timing()
{
	TIM1->PSC =
	TIM2->PSC =
	TIM3->PSC =
	TIM15->PSC =
	TIM16->PSC = PWM_PRESCALER; //делим входную частоту

	TIM1->ARR =
	TIM2->ARR =
	TIM3->ARR =
	TIM15->ARR =
	TIM16->ARR = PWM_SIZE; //задаём период ШИМ
}

//--------------------------------------------------
void configure_GPIO()
{
	//A: 0,1,2,3,6,7,8,11
	//               76543210
	GPIOA->CRL &= ~0xFF00FFFF; 						//очищаем
	GPIOA->CRL |=  0x99009999;            //af_output 10 MHz

	//               54321098
	GPIOA->CRH &= ~0x0000F00F;
	GPIOA->CRH |=  0x00009009;
	
	//B: 0,1,8,10,11,13,14,15
	//               76543210
	GPIOB->CRL &= ~0x000000FF;
	GPIOB->CRL |=  0x00000099;

	//               54321098
	GPIOB->CRH &= ~0xFFF0FF0F;
	GPIOB->CRH |=  0x99909909;

}

//--------------------------------------------------
void run_PWM_timers()
{
	TIM1->CR1 |= TIM_CR1_CEN; //запускаем таймеры
	TIM2->CR1 |= TIM_CR1_CEN;
	TIM3->CR1 |= TIM_CR1_CEN;
	TIM15->CR1 |= TIM_CR1_CEN;
	TIM16->CR1 |= TIM_CR1_CEN;	
}

//--------------------------------------------------
void configurePWM()
{
	connect_timers();

	TIM1->CR1 &= ~TIM_CR1_CEN; //останавливаем таймеры
	TIM2->CR1 &= ~TIM_CR1_CEN;
	TIM3->CR1 &= ~TIM_CR1_CEN;
	TIM15->CR1 &= ~TIM_CR1_CEN;
	TIM16->CR1 &= ~TIM_CR1_CEN;	
	
	set_pwm_timing();
	connect_PWM_channels();
	set_PWM_modes();
	
	configure_GPIO();
	
	run_PWM_timers();
}

//разводка ножек контроллера
#include "hal_0.h"

//--------------------------------------------------
//задаёт шим на выбранной катушке в пределах от -100 до 100% PWM_SIZE
//катушек 8
//у катушки один конец к ШИМ, второй заземляем
template <int inductor> void set_inductor_pulse_width (int len)
{
	if (len > 0) // == 0 не рассмотрен отдельно, поэтому два графика нормальные
	{
		//set_pulse_width<inductor*2+1>(0);   //deldebug  нужно только для отладки, чтобы рисовались нормальные графики
		enablePWM<inductor*2+1>(false);     //отключили от ШИМ
		set_pin_state<inductor*2+1>(false); //заземлили
		
		enablePWM<inductor*2>(true);        //подключили ШИМ
		set_pulse_width<inductor*2>(len);   //задали ширину импульсов
	}
	else
	{
		//set_pulse_width<inductor*2>(0);   //deldebug  нужно только для отладки
		enablePWM<inductor*2>(false);     //отключили от ШИМ
		set_pin_state<inductor*2>(false); //заземлили
		
		enablePWM<inductor*2+1>(true);        //подключили ШИМ
		set_pulse_width<inductor*2+1>(-len);   //задали ширину импульсов
	}
}
