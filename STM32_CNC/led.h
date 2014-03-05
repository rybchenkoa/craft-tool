#include "stm32f10x.h"

struct Led
{
//---------------------------------------
	void init()
	{
		RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
			//             54321098
		GPIOC->CRH &= ~0x00F00000;
		GPIOC->CRH |=  0x00100000;	
	}
//---------------------------------------
	void flip()
	{
		if(on)
			hide();
		else
			show();
	}
//---------------------------------------
	void show() { GPIOC->BRR=1<<13; on = true;}
//---------------------------------------
	void hide() { GPIOC->BSRR=1<<13; on = false;}	

	Led(): on(false){}
protected:
	bool on;
};

Led led;
