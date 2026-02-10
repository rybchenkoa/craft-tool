#pragma once
// чтение аналогового сигнала с пина

#include <hal/adc_ll.h>

class Adc
{
public:
	int channel = 0;
	
	inline uint32_t value()
	{
		// забираем результат прошлого измерения
		int result = adc_ll_rtc_get_convert_value(ADC_NUM_1);
		// запускаем новое измерение
		if (adc_ll_rtc_convert_is_done(ADC_NUM_1))
			adc_ll_rtc_start_convert(ADC_NUM_1, channel);
		
		// analogRead, adc1_get_raw, adc_hal_convert зависают на время измерения
		return result;
	}
	
	void init()
	{
		analogSetClockDiv(1);     // задаём делитель поменьше, чтобы измерять чаще
		analogReadResolution(12); // 12 бит
		adcAttachPin(channel);    // убираем другие функции с пина
		adc_ll_rtc_start_convert(ADC_NUM_1, channel);
	}
};

Adc adc;
