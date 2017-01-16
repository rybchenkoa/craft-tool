//#define STM32F10X_LD_VL
#include "stm32f10x.h"
#include "common.h"

//=====================================================================
//разводка выводов под двигатели
//номера сгруппированы по моторам
//[a0,a1], [a2,a3], [a6,a7], [b7,b9], [b6,b8]
//STEP [a0, a2, a6, b7, b6]
//DIR [a1, a3, a7, b9, b8]
//выводы ШИМ [b14, b15, a8, a11]

#define MAX_HARD_AXES 5
#define MAX_PWM  4

#define MAX_PERIOD 65535 //максимальное время между шагами для аппаратного таймера
#define MAX_STEP 65535 //16-битный счетчик шагов

int           STEP_PINS[]   = {0,     2,     6,     7,     6};
GPIO_TypeDef* STEP_PORTS[]  = {GPIOA, GPIOA, GPIOA, GPIOB, GPIOB};
int           DIR_PINS[]    = {1,     3,     7,     9,     8};
GPIO_TypeDef* DIR_PORTS[]   = {GPIOA, GPIOA, GPIOA, GPIOB, GPIOB};
TIM_TypeDef*  STEP_TIMERS[] = {TIM2,  TIM15, TIM3,  TIM17, TIM16};
//int STEP_CHANNELS[] = [1, 1, 1, 1, 1] у всех step таймеров работает первый канал сравнения
bool          CHANNEL_NEG[] = {false, false, false, true, true};

//канал DMA нужен только для того, чтобы использоваться в качестве счетчика импульсов таймера
//RM0041 стр. 149 , каналы 2, 5, 3, 7, 6, событие TIM_UP
DMA_Channel_TypeDef* DMA_CHANNELS[] = {DMA1_Channel2, DMA1_Channel5, DMA1_Channel3, DMA1_Channel7, DMA1_Channel6};
char dummy;

//входы b2, b10, b11, b12, a12, a15, b3, b4, b5
GPIO_TypeDef* IN_PINS[]  = {GPIOB, GPIOB, GPIOB, GPIOB, GPIOA, GPIOA, GPIOB, GPIOB, GPIOB};
int           IN_PORTS[] = {2,        10,    11,    12,    12,    15,     3,     4,     5};
//--------------------------------------------------
inline void gpio_port_crl(GPIO_TypeDef *port, unsigned int mask, unsigned int val)
{
	int reg = port->CRL;
	reg &= ~(mask * 0xF); //очищаем
	reg |=  mask * val;
	port->CRL = reg;
}

//--------------------------------------------------
inline void gpio_port_crh(GPIO_TypeDef *port, unsigned int mask, unsigned int val)
{
	int reg = port->CRH;
	reg &= ~(mask * 0xF); //очищаем
	reg |=  mask * val;
	port->CRH = reg;
}

#define MANUAL_OUT 1
#define AF_OUT     9
#define PULL_UP_IN 12
//--------------------------------------------------
void configure_gpio()
{
	//сначала STEP и DIR управляются вручную, задаем 1 - output 10 MHz
	//DIR управляется вручную, для него задаем 1 - output 10 MHz
	//STEP управляется таймерами, для него надо 9 - af_output 10 MHz
	
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
	RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
	
	//DIR A: 1, 3, 7
	//                     76543210
	gpio_port_crl(GPIOA, 0x10001010, MANUAL_OUT);
	//DIR B: 9, 8
	//                     54321098
	gpio_port_crh(GPIOB, 0x00000011, MANUAL_OUT);
	
	//STEP A: 0, 2, 6,
	//                     76543210
	gpio_port_crl(GPIOA, 0x01000101, AF_OUT);
	//STEP B: 7, 6
	//                     76543210
	gpio_port_crl(GPIOB, 0x11000000, AF_OUT);
	
	//PWM A: 8, 11
	//                     54321098
	gpio_port_crh(GPIOA, 0x00001001, AF_OUT);
	
	//PWM B: 14, 15
	//                     54321098
	gpio_port_crh(GPIOA, 0x11000000, AF_OUT);
	
	//входы a12, a15
	//                     54321098
	gpio_port_crh(GPIOA, 0x10010000, PULL_UP_IN);
	
	//входы b2, b10, b11, b12, b3, b4, b5
	//                     76543210
	gpio_port_crl(GPIOB, 0x00111100, PULL_UP_IN);
	//                     54321098
	gpio_port_crh(GPIOB, 0x00011100, PULL_UP_IN);
}


//--------------------------------------------------
// включает тактирование портов и таймеров
void connect_timers()
{
	RCC->APB2ENR |= RCC_APB2ENR_AFIOEN
								| RCC_APB2ENR_IOPAEN
								| RCC_APB2ENR_IOPBEN
								| RCC_APB2ENR_TIM1EN
								| RCC_APB2ENR_TIM15EN
								| RCC_APB2ENR_TIM16EN
								| RCC_APB2ENR_TIM17EN;
								
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN
								| RCC_APB1ENR_TIM3EN;
}

//--------------------------------------------------
void config_step_timer(TIM_TypeDef *tim, DMA_Channel_TypeDef* dma, bool isNeg)
{
	tim->CR1 &= ~TIM_CR1_CEN; //останавливаем счётчик
	tim->CCMR1  = TIM_CCMR1_OC1M_0 | TIM_CCMR1_OC1M_1; //toggle режим
	tim->BDTR |= TIM_BDTR_MOE; //убираем блокировку выходов, описание в RM0041, стр. 235, 264

	tim->ARR = 65000; //(минимальная частота = 2)на каждом апдейте меняем полярность выхода
	tim->CNT = 0;
	tim->CCR1 = 0; //выход переключается, когда значение совпадает, пусть это происходит в конце
	tim->CR1 |= TIM_CR1_ARPE; //сравнение CNT == ARR, поэтому чтобы не пролететь мимо во время обновления
	
	//включаем выход
	if (!isNeg)
		tim->CCER |= TIM_CCER_CC1E | TIM_CCER_CC1P; //TIM_CCER_CC1P - обратная полярность
	else
		tim->CCER |= TIM_CCER_CC1NE; //TIM_CCER_CC1NP

	tim->DIER |= TIM_DIER_UDE; //запуск DMA при совпадении с ARR

	dma->CCR |= DMA_CCR1_CIRC;
	dma->CNDTR = MAX_STEP;
	dma->CMAR = (int)&dummy;
	dma->CPAR = (int)&TIM1->CNT;	
	dma->CCR |= DMA_CCR1_EN;
}

//--------------------------------------------------
//задаёт режим сравнения для таймеров
void config_pwm_timer()
{
	TIM1->CR1 &= ~TIM_CR1_CEN;
	TIM1->CCMR1 = TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2   //левый шим на всех каналах
							| TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2;

	TIM1->CCMR2 = TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2
							| TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2;

	TIM1->BDTR |= TIM_BDTR_MOE;
	TIM1->CCER |= TIM_CCER_CC1E
							| TIM_CCER_CC2NE | TIM_CCER_CC2NP
							| TIM_CCER_CC3NE | TIM_CCER_CC3NP
							| TIM_CCER_CC4E; //подключаем все каналы для шим

	TIM1->PSC = 0;
	TIM1->ARR = PWM_SIZE;
	TIM1->CR1 |= TIM_CR1_ARPE; //чтобы при обновлении с большего на меньшее не считать до 2^16
	TIM1->CR1 |= TIM_CR1_CEN; //запускаем таймер
}


//--------------------------------------------------
//подключение каналов шим
void connect_step_channels()
{
	//RM0041, стр 114
	//AFIO->MAPR |= AFIO_MAPR_TIM2_REMAP_1; //перекидываем tim2 ch3,ch4 на b10, b11

	for (int i = 0; i < MAX_HARD_AXES; ++i)
		config_step_timer(STEP_TIMERS[i], DMA_CHANNELS[i], CHANNEL_NEG[i]);
}


//--------------------------------------------------
void configure_timers()
{
	connect_timers();
	connect_step_channels();
	config_pwm_timer();
}


//--------------------------------------------------
//--------------------------------------------------
//задаёт состояние пина порта
void __forceinline set_pin_state(GPIO_TypeDef *port, int pinnum, bool state)
{
	if (state)
		port->BSRR = 1 << pinnum;
	else
		port->BRR  = 1 << pinnum;
}

//--------------------------------------------------
//включение/выключение управления ножкой порта с помощью периферии
#define USE_AF 0x8
void __forceinline set_port_af(GPIO_TypeDef *port, int pinnum, bool state)
{
	if (pinnum < 8)
	{
		if (state)
			port->CRL |= USE_AF * (1<<(pinnum*4)); //на каждый пин 4 бита
		else
			port->CRL &= ~(USE_AF * (1<<(pinnum*4)));
	}
	else
	{
		pinnum -= 8;
		if (state)
			port->CRH |= USE_AF * (1<<(pinnum*4));
		else
			port->CRH &= ~(USE_AF * (1<<(pinnum*4)));
	}
}

//--------------------------------------------------
//задает направление
void inline set_dir(int index, bool state)
{
	set_pin_state(DIR_PORTS[index], DIR_PINS[index], state);
}

//--------------------------------------------------
//задает направление
bool inline get_pin(int index)
{
	return IN_PINS[index]->IDR | (1 << IN_PORTS[index]);
}

//--------------------------------------------------
//задает время между шагами
void inline set_step_time(int index, int ticks)
{
	STEP_TIMERS[index]->ARR = ticks;
}

//--------------------------------------------------
//включает автошаги
void inline enable_step_timer(int index)
{
	STEP_TIMERS[index]->CR1 |= TIM_CR1_CEN;
	//set_port_af(STEP_PORTS[index], STEP_PINS[index], true);
}

//--------------------------------------------------
//выключает автошаги
void inline disable_step_timer(int index)
{
	STEP_TIMERS[index]->CR1 &= ~TIM_CR1_CEN;
	//set_port_af(STEP_PORTS[index], STEP_PINS[index], false);
}

//--------------------------------------------------
//возвращает число шагов, сделанных таймером
int inline get_steps(int index)
{
	return -DMA_CHANNELS[index]->CNDTR;
}

//--------------------------------------------------
//делает шаг таймером. (делает cnt!=ccr1, а потом равным (одновременно запуская dma)
void inline step(int index)
{
	STEP_TIMERS[index]->CNT = 1; //STEP_TIMERS[index]->ARR;//STEP_TIMERS[index]->CCR1;
	STEP_TIMERS[index]->EGR = TIM_EGR_UG/* | TIM_EGR_CC1G*/;
}

//--------------------------------------------------
//выставляет время до следующего шага
void inline set_next_step_time(int index, int time)
{
	time = STEP_TIMERS[index]->ARR - time;
	if (time < 1)
		time = 1;
	STEP_TIMERS[index]->CNT = time;
}
