#include "stm32f10x.h"
#include "led.h"
#include "gpio_use.h"
#include "fifo.h"
#include "USART_module.h"
#include "adc.h"
#include "stdio.h"
#include "string.h"
#include "sys_timer.h"
#include "mover.h"

//===============================================================
bool __forceinline const_pin_state()
{
	return !(GPIOA->IDR & (1<<13));
}

bool __forceinline float_pin_state()
{
	return !(GPIOA->IDR & (1<<12));
}

void init_motors()
{
	//               54321098
	GPIOA->CRH &= ~0x00FF0000;  //контакты для измерения времени достижения заданного напряжения
	
	timer.delay_ms(100); //ждём пока питание стабилизируется
	
	for (int i=0; i < COUNT_DRIVES; ++i) //то, что итераторы не имеют оператора "<" - это их недостаток
	{
		motor[i].index = i;
		motor[i].maxVoltage = 255;
		//сюда же запихнуть измерение индуктивности/сопротивления обмоток
		//led.hide();
		/*
		for (int numCoils = 0; numCoils < 2; numCoils++)
		{
			if (numCoils == 0)
				motor[i].set_coils_PWM(PWM_SIZE, 0); //подаём напряжение
			else
				motor[i].set_coils_PWM(0, PWM_SIZE);
				
			int endTime = timer.get_ms(500);             //и засекаем время
			
			int timeConst = 0, timeFloat = 0; //время сравнения с диодом, сравнения с питающим напряжением
			
			while(timer.check(endTime) && (timeConst == 0 || timeFloat == 0)) //пока есть время для определения параметров, определяем
			{
				if (timeConst == 0 && const_pin_state())
				{
					timeConst = maxDelay - SysTick->VAL;
					if (timeFloat != 0)
						break;
				}
				
				if (timeFloat == 0 && float_pin_state())
				{
					timeFloat = maxDelay - SysTick->VAL;
					if (timeConst != 0)
						break;
				}
			}
			
			motor[i].set_coils_PWM(0, 0); //отключаем напряжение
			
			log_console("%i %i\r\n", timeConst, timeFloat);
			
			char test[4] = {6,0,0,0};
			log_console("%x", calc_crc(test, 1));
			
			//if (timeConst == 0 && timeFloat == 0)
				//return;

			endTime = timer.get_ms(500);
			while(timer.check(endTime) && (const_pin_state() || float_pin_state()));
		}
		
		//led.flip();
		*/
	}
}

//------------------------------------------------------
void init()
{
	led.init();	
	led.hide();
	timer.init();
	
	RCC->AHBENR |= RCC_AHBENR_CRCEN;
	
	usart.init();
	
	receiver.init();
	
	configurePWM();
	
	init_motors(); //уже использует ШИМ
	
	adc.init();
}

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

int main()
{
	init();
	led.show();

	mover.init();
	int timeToSend = timer.get_ms(500);
	int busyTime = 0; //запаздывание обработки следующего тика (джиттер)
	int stepTime = 0; //необходимое время между двумя шагами
	while(1)
	{

		if(!timer.check(mover.stopTime))
		{
			busyTime = mover.stopTime - timer.get();
			stepTime = mover.stopTime;
			mover.update();
			stepTime -= mover.stopTime;
		}

		if(!timer.check(timeToSend))
		{
			led.flip();
			timeToSend = timer.get_ms(100);
			//log_console("dest %d %d %d, sc %d\n", mover.to[0], mover.to[1], mover.to[2], int(float16(1)/mover.circleData.scale2));
			//log_console("stepT %d, jitter %d, st %d\n", stepTime, busyTime, mover.linearData.state);
			log_console("adc %d\n", adc.value());
			send_packet_service_coords(mover.coord);
		}
	}
}


