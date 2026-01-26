//ǁ
#include "stm32f407xx.h"
class Adc
{
	public:
	inline uint32_t value()
	{
		return ADC3->DR;
	}
	
	void init()
	{
        LL_GPIO_InitTypeDef gpio;
        gpio.Pin = LL_GPIO_PIN_3;
        gpio.Mode = LL_GPIO_MODE_ANALOG;
        gpio.Pull = LL_GPIO_PULL_NO;
        LL_GPIO_Init(GPIOF, &gpio);

        ADC_TypeDef *adc = ADC3;
		adc->CR2 |= ADC_CR2_ADON;            //включаем сам adc
		//adc->SR & ADC_SR_STRT              //ждём, пока заведётся
		
		adc->SQR1 &= ~(ADC_SQR1_L);          //меряем один канал
		adc->SQR3 |= ADC_SQR3_SQ1_0 * 9;     //первым будем мерять четвёртый канал
		/*
		adc->JSQR &= ~(ADC_JSQR_JL_0 | ADC_JSQR_JL_1); //используем один выделенный канал
		adc->JSQR |= ADC_JSQR_JSQ1_0 * 4;    //первым будем мерять четвёртый канал
		*/
		
		adc->CR2 |= ADC_CR2_CONT;            //постоянно
		adc->SMPR1 = 0;                      //с максимальной частотой

		/*
		adc->CR1 |= ADC_CR1_SCAN;
		adc->CR2 |= ADC_CR2_JSWSTART;        //шлём сигнал о начале измерений
		*/
		adc->CR2 |= ADC_CR2_EXTSEL;         //старт по SWSTART
		adc->CR2 |= ADC_CR2_EXTEN;        //
		adc->CR2 |= ADC_CR2_SWSTART;        //шлём сигнал о начале измерений
	}
};

Adc adc;
