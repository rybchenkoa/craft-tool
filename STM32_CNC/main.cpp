#include "stm32f10x.h"
#include "led.h"
#include "gpio_use.h"
#include "motor.h"
#include "fifo.h"
#include "USART_module.h"

Motor motor[COUNT_DRIVES];

//===============================================================

void init_motors()
{
	for (int i=0; i < COUNT_DRIVES; ++i) //то, что итераторы не имеют оператора "<" - это их недостаток
	{
		motor[i].index = i;
		motor[i].maxVoltage = 128;
		//сюда же запихнуть измерение индуктивности/сопротивления обмоток
	}
}

//------------------------------------------------------
void init_SysTick()
{
	SysTick->LOAD  = SysTick_LOAD_RELOAD_Msk - 1;      /* set reload register */
	SysTick->CTRL  = SysTick_CTRL_CLKSOURCE_Msk |
                   SysTick_CTRL_ENABLE_Msk;                    /* Enable SysTick Timer */
}

//------------------------------------------------------
void init()
{
	init_SysTick();
	
	usart.init();
	
	configurePWM();
	
	init_motors(); //уже использует ШИМ
	
	led.init();
}

//---------------------------------------
struct Coord
{
	int x,y,z; //текущие координаты
};

struct PointTo
{
	int x;    //куда доехать
	int y;    //измеряется в шагах
	int z;
	int time; //за сколько доехать
};


struct PointTo Path[10];   //путь фрезы

struct Coord curPos;       //текущее положение резца

//---------------------------------------
const bool ENABLE_VAL = true;
//const bool PIN_VAL = false;
void enable_all_pwms(int PWM_VAL)
{
	enablePWM<0>(ENABLE_VAL);
	set_pulse_width<0>(PWM_VAL);
	
	enablePWM<1>(ENABLE_VAL);
	set_pulse_width<1>(PWM_VAL);
	
	enablePWM<2>(ENABLE_VAL);
	set_pulse_width<2>(PWM_VAL);
	
	enablePWM<3>(ENABLE_VAL);
	set_pulse_width<3>(PWM_VAL);
	
	enablePWM<4>(ENABLE_VAL);
	set_pulse_width<4>(PWM_VAL);
	
	enablePWM<5>(ENABLE_VAL);
	set_pulse_width<5>(PWM_VAL);
	
	enablePWM<6>(ENABLE_VAL);
	set_pulse_width<6>(PWM_VAL);
	
	enablePWM<7>(ENABLE_VAL);
	set_pulse_width<7>(PWM_VAL);
	
	enablePWM<8>(ENABLE_VAL);
	set_pulse_width<8>(PWM_VAL);
	
	enablePWM<9>(ENABLE_VAL);
	set_pulse_width<9>(PWM_VAL);
	
	enablePWM<10>(ENABLE_VAL);
	set_pulse_width<10>(PWM_VAL);
	
	enablePWM<11>(ENABLE_VAL);
	set_pulse_width<11>(PWM_VAL);
	
	enablePWM<12>(ENABLE_VAL);
	set_pulse_width<12>(PWM_VAL);
	
	enablePWM<13>(ENABLE_VAL);
	set_pulse_width<13>(PWM_VAL);
	
	enablePWM<14>(ENABLE_VAL);
	set_pulse_width<14>(PWM_VAL);
	
	enablePWM<15>(ENABLE_VAL);
	set_pulse_width<15>(PWM_VAL);
	/*
	set_pin_state<0>(PIN_VAL);
	set_pin_state<1>(PIN_VAL);
	set_pin_state<2>(PIN_VAL);
	set_pin_state<3>(PIN_VAL);
	set_pin_state<4>(PIN_VAL);
	set_pin_state<5>(PIN_VAL);
	set_pin_state<6>(PIN_VAL);
	set_pin_state<7>(PIN_VAL);
	set_pin_state<8>(PIN_VAL);
	set_pin_state<9>(PIN_VAL);
	set_pin_state<10>(PIN_VAL);
	set_pin_state<11>(PIN_VAL);
	set_pin_state<12>(PIN_VAL);
	set_pin_state<13>(PIN_VAL);
	set_pin_state<14>(PIN_VAL);
	set_pin_state<15>(PIN_VAL);
	*/
}
//--------------------------------------------------------
void inline wait_sys_tick(uint32_t time)
{
	SysTick->LOAD = time;
	SysTick->VAL = 0;
	__nop();
	SysTick->LOAD = SysTick_LOAD_RELOAD_Msk - 1;
	while(SysTick->VAL > 10);	
}

void delay_mss(int mss) //micro_santi_sec :)
{
	static const uint32_t maxDelay = SysTick_LOAD_RELOAD_Msk >> 2;
	uint32_t takts = mss*240;
	while (takts > maxDelay)
	{
		takts -= maxDelay;
		wait_sys_tick(maxDelay);
	}
		
	wait_sys_tick(takts);
}

int main()
{
	init();

	int coord = 0;
	
	while(1)
	for (int i=0;i<PWM_SIZE;i++)
	{
		delay_mss(100);
		//enable_all_pwms(i);
		coord++;
		for (int j=0;j<4;j++) //400 тактов на задание напряжения всех двигателей
		//int j=1;
			motor[j].set_sin_voltage(coord, 128);
	}
	/*for (int i=0;i<4;i++)
		motor[i].set_coils_PWM(0, -PWM_SIZE/1.2);*/
	/*
	while(1)
	{
		if(TIM16->CNT > TIM16->CCR1)
			led.show();
		else
			led.hide();
		//led.flip();
	}
	*/
}


