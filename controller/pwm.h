#pragma once
// ручной и аппаратный ШИМ

#include "gpio.h"
#include "packets.h"


class PwmController
{
public:
	//параметры для медленного ШИМ
	int pwmSlowPeriod;
	int lastPwmUpdate;
	int pwmSizes[MAX_SLOW_PWMS];

	//---------------------------------------------------------------------
	PwmController()
	{
		pwmSlowPeriod = PWM_SLOW_SIZE;
		for(int i = 0; i < MAX_SLOW_PWMS; i++)
			pwmSizes[i] = 0;
		lastPwmUpdate = 0;
	}
	
	//---------------------------------------------------------------------
	void update(int time)
	{
		int delta = time - lastPwmUpdate;
		if (delta >= pwmSlowPeriod)
		{
			lastPwmUpdate = time;
			delta = 0;
		}
		for (int i = 0; i < MAX_SLOW_PWMS; ++i)
			set_pwm_pin(i, delta < pwmSizes[i]);
	}

	//---------------------------------------------------------------------
	// задаёт ширину импульсов для одного канала
	void process_packet_set_pwm(PacketSetPWM *packet)
	{
		if (packet->pin < MAX_SLOW_PWMS + MAX_PWM)
		{
			if (packet->pin < MAX_SLOW_PWMS)
				pwmSizes[(int)packet->pin] = pwmSlowPeriod * packet->value;
			else
				set_pwm_width(MAX_SLOW_PWMS - packet->pin, TIM1->ARR * packet->value);
		}
	}

	//---------------------------------------------------------------------
	// задаёт частоту импульсов для всех каналов
	void process_packet_set_pwm_freq(PacketSetPWMFreq *packet)
	{
		pwmSlowPeriod = 1000000 / packet->slowFreq;
		set_pwm_period(CORE_FREQ / packet->freq);
	}
};
