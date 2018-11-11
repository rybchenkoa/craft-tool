#include "stm32f407xx.h"
#include "common.h"

/*
импульсы ШИМ можно считать вторым таймером (1 столбец)
остальные столбцы - это таймеры, от которых ловим импульсы
RM00090 567, 632, 674
1	5	2	3	4		//этот для шим, его не трогаем
8	1	2	4	5

2	1	8	3	4
3	1	2	5	4
4	1	2	3	8		// 4<-2
5	2	3	4	8		// 5<-3

9	2	3	10	11		// 9<-10
12	4	5	13	14		// 12<-13
и того 4 таймера

еще можно считать импульсы таймера счетчиком, который находится в канале DMA
при этом будут делаться паразитные посылки по шине данных

или сделать под каждый канал пдп массив
это лишняя память, т.к. на 8бит счетчик надо 1кБ памяти, работает только с APB шиной
//RM00090  308, 652
1,0	5			// 5  DMA1 stream0, TIM5
1,1	2	6		// 6
1,2	3	7		// 3
1,4	7			// 7
1,6	4	5		// 4
1,7	2			// 2

второй поток может управлять через BSRR
2,1	8			// 8
2,2	u1_rx
2,5	1	u1_rx
2,7	u1_tx

tim4 - выходы как замена bsrr
6,7 - только через dma
9,12 в любом случае через каскадирование таймеров - 2 канала

*/


//=====================================================================
//разводка выводов под двигатели
//STEP [d12, d13, d14, d15, g4]
//DIR [d10, d11, g2, g3, g6]
//выводы ШИМ []

#define MAX_HARD_AXES 5
#define MAX_PWM  4

#define MAX_PERIOD 65535 //16-бит максимальное время между шагами для аппаратного таймера
#define MAX_STEP 256 //8-битный счетчик шагов, 0 пропускается, вместо него 0x1 00

int           DIR_PINS[]    = {10,    11,    2,     3,     6};
GPIO_TypeDef* DIR_PORTS[]   = {GPIOD, GPIOD, GPIOG, GPIOG, GPIOG};
TIM_TypeDef*  STEP_TIMERS[] = {TIM2,  TIM3, TIM6,  TIM7, TIM8};
DMA_Stream_TypeDef* DMA_STREAMS[] = {DMA1_Stream7, DMA1_Stream2, DMA1_Stream1, DMA1_Stream4, DMA2_Stream1};
int DMA_TRIGGERS[] = {DMA_CHANNEL_3, DMA_CHANNEL_5, DMA_CHANNEL_7, DMA_CHANNEL_1, DMA_CHANNEL_7};

//входы a15, c10, c11, c12, d0, d1, d2, d3, d4
GPIO_TypeDef* IN_PORTS[] = {GPIOA, GPIOC, GPIOC, GPIOC, GPIOD, GPIOD, GPIOD, GPIOD, GPIOD};
int           IN_PINS[]  = {15,        10,    11,    12,    0,    1,     2,     3,     4};
int polarity = 0; //надо ли инвертировать вход (битовый массив)

char OC_ARRAY[MAX_STEP];
int BSRR_ARRAY[MAX_STEP];


//--------------------------------------------------
// включает тактирование портов и таймеров
void connect_timers_clock()
{
  	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN
                  | RCC_AHB1ENR_GPIOCEN
                  | RCC_AHB1ENR_GPIODEN
                  | RCC_AHB1ENR_GPIOGEN
                  | RCC_AHB1ENR_DMA1EN
                  | RCC_AHB1ENR_DMA2EN;

	RCC->APB2ENR |= RCC_APB2ENR_TIM8EN;
								
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN
				  | RCC_APB1ENR_TIM3EN
                  | RCC_APB1ENR_TIM4EN
                  | RCC_APB1ENR_TIM6EN
                  | RCC_APB1ENR_TIM7EN;
}

//--------------------------------------------------
void configure_gpio()
{
    LL_GPIO_InitTypeDef gpio;
	gpio.Mode = LL_GPIO_MODE_INPUT;
	gpio.Pull = LL_GPIO_PULL_UP;
    for (int i = 0; i < 9; ++i)
    {
	  gpio.Pin = 1<<IN_PINS[i];
	  LL_GPIO_Init(IN_PORTS[i], &gpio);
    }

    //DIR управляется вручную
	gpio.Mode = LL_GPIO_MODE_OUTPUT;
	gpio.Speed = LL_GPIO_SPEED_FREQ_LOW;
	gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    for (int i = 0; i < MAX_HARD_AXES; ++i)
    {
	  gpio.Pin = 1<<DIR_PINS[i];
	  LL_GPIO_Init(DIR_PORTS[i], &gpio);
    }
	
    //'этот STEP через BSRR
	gpio.Pin = LL_GPIO_PIN_4;
	LL_GPIO_Init(GPIOG, &gpio);

    // эти STEP управляется таймерами
    gpio.Pin = GPIO_PIN_9;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Alternate = GPIO_AF2_TIM4;
    LL_GPIO_Init(GPIOD, &gpio);
}

//--------------------------------------------------
// таймер, у которого каналы сравнения используются для управления ножками
// хорош тем, что висит на шине, доступной для обращения через DMA
void config_output_timer(TIM_TypeDef *tim, char *arr)
{
  for (int i = 0; i < MAX_STEP; i+=2)
  {
	arr[i] = -1;
	arr[i+1] = 0;
  }

  TIM_TypeDef *timOut = TIM14;
  timOut->CNT = 1;
  timOut->ARR = 2;
  timOut->CCMR1 |= LL_TIM_OCMODE_PWM1*(1 + (1<<8)); // постоянное сравнение на <=
  timOut->CCMR2 |= LL_TIM_OCMODE_PWM1*(1 + (1<<8));
  timOut->BDTR |= TIM_BDTR_MOE; //убираем блокировку выходов
  timOut->CCER |= TIM_CCER_CC1E //включаем все 4 канала сравнения
                | TIM_CCER_CC2E
                | TIM_CCER_CC3E
                | TIM_CCER_CC4E;
}

//--------------------------------------------------
//таймер, который просто генерирует запросы к DMA с нужной частотой
void config_step_timer(TIM_TypeDef *tim)
{
  tim->CNT = 0;
  tim->ARR = 0;
  //tim->CR1 |= TIM_CR1_ARPE; //сравнение CNT == ARR, поэтому чтобы не пролететь мимо во время обновления
  tim->DIER |= TIM_DIER_UDE; //разрешаем DMA запрос по событию переполнения (совпадение с ARR)
}

//--------------------------------------------------
// DMA, который дергает ножкой через BSRR
// на каждую ножку нужен отдельный массив
void dma_bsrr_stream(DMA_Stream_TypeDef *stream, int channel,
                    GPIO_TypeDef *port, int* arr, int bitSet, int bitReset)
{
  for (int i = 0; i < MAX_STEP; i+=2)
  {
	arr[i] = bitSet;
	arr[i+1] = bitReset;
  }

  stream->CR |= DMA_MEMORY_TO_PERIPH
    | DMA_SxCR_CIRC
    | DMA_MDATAALIGN_WORD | DMA_PDATAALIGN_WORD
    | DMA_SxCR_MINC //увеличивать указатель на память
    | channel; //тактирование брать с этого триггера
  stream->NDTR = 255;
  stream->M0AR = (int)arr;
  stream->PAR = (int)&GPIOF->BSRR;
  stream->CR |= DMA_SxCR_EN;
}

//--------------------------------------------------
// DMA, который переключает ножку через канал сравнения таймера
void dma_oc_stream(int *outReg, DMA_Stream_TypeDef *stream, int channel, char *arr)
{
  DMA_Stream_TypeDef *stream = DMA1_Stream1;
  stream->CR |= DMA_MEMORY_TO_PERIPH
    | DMA_SxCR_CIRC
    | DMA_MDATAALIGN_BYTE | DMA_PDATAALIGN_WORD
    | DMA_SxCR_MINC //увеличивать указатель на память
    | channel; //тактирование брать с этого триггера
  stream->NDTR = 256;
  stream->M0AR = (int)arr;
  stream->PAR = (int)&outReg;	
  stream->CR |= DMA_SxCR_EN;
}

//--------------------------------------------------
//один таймер для каналов ШИМ
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
//подключение шаговых таймеров
void connect_step_channels()
{
	for (int i = 0; i < MAX_HARD_AXES; ++i) //настраиваем таймера, генерирующие шаги
		config_step_timer(STEP_TIMERS[i]);

    config_output_timer(TIM4, OC_ARRAY); //включаем таймер, шим каналы которого управляют ножками
    for (int i = 0; i < 4; ++i)
      dma_oc_stream(&TIM4->CCR1 + i, DMA_STREAMS[i], DMA_TRIGGERS[i], OC_ARRAY);

    dma_bsrr_stream(DMA_STREAMS[4], DMA_TRIGGERS[4], GPIOG, BSRR_ARRAY, GPIO_BSRR_BR4, GPIO_BSRR_BS4); 
}

//--------------------------------------------------
void configure_timers()
{
	connect_timers_clock();
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
		port->BSRR  = 1 << (pinnum + 16);
}

//--------------------------------------------------
//задает направление
void inline set_dir(int index, bool state)
{
	set_pin_state(DIR_PORTS[index], DIR_PINS[index], state);
}

//--------------------------------------------------
//получает сигнал на ножке
bool inline get_pin(int index)
{
	int pinNum = IN_PINS[index];
	return ((IN_PORTS[index]->IDR >> pinNum) ^ (polarity >> index)) & 1;
}

//--------------------------------------------------
//задает время между шагами
void inline set_step_time(int index, int ticks)
{
	STEP_TIMERS[index]->ARR = ticks + 1;
}

//--------------------------------------------------
//включает автошаги
void inline enable_step_timer(int index)
{
	STEP_TIMERS[index]->CR1 |= TIM_CR1_CEN;
}

//--------------------------------------------------
//выключает автошаги
void inline disable_step_timer(int index)
{
	STEP_TIMERS[index]->CR1 &= ~TIM_CR1_CEN;
}

//--------------------------------------------------
//возвращает число шагов, сделанных таймером
int inline get_steps(int index)
{
	return -DMA_STREAMS[index]->NDTR;
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
