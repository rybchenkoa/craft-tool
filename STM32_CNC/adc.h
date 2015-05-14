
class Adc
{
	public:
	inline uint32_t value()
	{
		return ADC1->DR;
	}
	
	void init()
	{
		//A: 0,1,2,3,6,7,8,11
		//               76543210
		GPIOA->CRL &= ~0x000F0000; 						//включаем
		//GPIOA->CRL |=  0x00000000;          //analog input
		
		RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;   //подключаем тактирование adc
		ADC1->CR2 |= ADC_CR2_ADON;            //включаем сам adc
		//ADC1->SR & ADC_SR_STRT              //ждём, пока заведётся
		
		ADC1->SQR1 &= ~(ADC_SQR1_L);          //меряем один канал
		ADC1->SQR3 |= ADC_SQR3_SQ1_0 * 4;     //первым будем мерять четвёртый канал

		/*
		ADC1->JSQR &= ~(ADC_JSQR_JL_0 | ADC_JSQR_JL_1); //используем один выделенный канал
		ADC1->JSQR |= ADC_JSQR_JSQ1_0 * 4;    //первым будем мерять четвёртый канал

		*/
		
		ADC1->CR2 |= ADC_CR2_CONT;            //постоянно
		ADC1->SMPR1 = 0;                      //с максимальной частотой

		/*
		ADC1->CR1 |= ADC_CR1_SCAN;
		ADC1->CR2 |= ADC_CR2_JSWSTART;        //шлём сигнал о начале измерений
		*/
		ADC1->CR2 |= ADC_CR2_EXTSEL;         //старт по SWSTART
		ADC1->CR2 |= ADC_CR2_EXTTRIG;        //
		ADC1->CR2 |= ADC_CR2_SWSTART;        //шлём сигнал о начале измерений
	}
};

Adc adc;
