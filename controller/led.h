//ǁ
#pragma once
#include "stm32f4xx.h"

static const int pos[] = {9, 10};
struct Led
{
//---------------------------------------
	void init()
	{
		LL_GPIO_InitTypeDef gpio;
		gpio.Pin = LL_GPIO_PIN_9 | LL_GPIO_PIN_10;
	    gpio.Mode = LL_GPIO_MODE_OUTPUT;
	    gpio.Speed = LL_GPIO_SPEED_FREQ_LOW;
	    gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	    LL_GPIO_Init(GPIOF, &gpio);
	}
//---------------------------------------
	void flip(int i)
	{
		if(on[i])
			hide(i);
		else
			show(i);
	}
//---------------------------------------
	void show(int i) { GPIOF->BSRR=1<<(pos[i]+16); on[i] = true;}
//---------------------------------------
	void hide(int i) { GPIOF->BSRR=1<<pos[i]; on[i] = false;}

	Led() {on[0]=on[1]=false;}
protected:
	bool on[2];
};

Led led;
