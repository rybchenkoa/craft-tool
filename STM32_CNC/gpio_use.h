//#define STM32F10X_LD_VL
#include "stm32f10x.h"
#include "common.h"
//=====================================================================
//разводка выводов под двигатели
//номера сгруппированы по моторам
//[a0,a1], [a2,a3], [a6,a7], [b7,b6]

//--------------------------------------------------
void configure_GPIO()
{
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
	
	//A: 0,1,2,3,6,7,8,9
	//               76543210
	GPIOA->CRL &= ~0xFF00FFFF; 						//очищаем
	GPIOA->CRL |=  0x11001111;            //output 10 MHz

	//               54321098
	GPIOA->CRH &= ~0x0000000F;
	GPIOA->CRH |=  0x00000001;
	
	
	RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
	//               76543210
	GPIOB->CRL &= ~0xFF000000; 						//очищаем
	GPIOB->CRL |=  0x11000000;            //output 10 MHz

}



//--------------------------------------------------
//задаёт состояние пина порта
void __forceinline set_pin_state(GPIO_TypeDef *port, int pinnum, bool state)
{
	if (state)
		port->BSRR = 1 << pinnum;
	else
		port->BRR  = 1 << pinnum;
}

template <int pin> void set_pin_state(bool state) {}

//a0
template <> void set_pin_state<0> (bool state) { set_pin_state(GPIOA, 0, state); }
//a1
template <> void set_pin_state<1> (bool state) { set_pin_state(GPIOA, 1, state); }

//a2
template <> void set_pin_state<2> (bool state) { set_pin_state(GPIOA, 2, state); }
//a3
template <> void set_pin_state<3> (bool state) { set_pin_state(GPIOA, 3, state); }

//a6
template <> void set_pin_state<4> (bool state) { set_pin_state(GPIOA, 6, state); }
//a7
template <> void set_pin_state<5> (bool state) { set_pin_state(GPIOA, 7, state); }

//b7
template <> void set_pin_state<6> (bool state) { set_pin_state(GPIOB, 7, state); }
//b6
template <> void set_pin_state<7> (bool state) { set_pin_state(GPIOB, 6, state); }

