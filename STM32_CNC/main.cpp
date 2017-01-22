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
void init_motors()
{
	for (int i=0; i < MAX_HARD_AXES; ++i)
		motor[i]._index = i;
}

//===============================================================
void init()
{
	led.init();	
	led.hide();
	timer.init();
	
	RCC->AHBENR |= RCC_AHBENR_CRCEN;
	
	usart.init();
	receiver.init();
	configure_gpio();
	configure_timers();
	init_motors();
	adc.init();
	
	DBGMCU->CR = -1u; //при отладке останавливаем всё
}

//===============================================================
int main()
{
	init();
	led.show();

	mover.init();
	int timeToSend = timer.get_ms(500);
	int stepTime; //время обработки одного шага
	mover.canLog = true;
	while(1)
	{
		stepTime = timer.get();
		mover.update();
		stepTime = timer.get() - stepTime;

		if(!timer.check(timeToSend))
		{
			led.flip();
			timeToSend = timer.get_ms(100);
			//log_console("dest %d %d %d, sc %d\n", mover.to[0], mover.to[1], mover.to[2]);
			//log_console("stepT %d, st %d\n", stepTime, mover.linearData.state);
			//log_console("mot %d, %d, %d\n", motor[0]._isHardware, motor[1]._isHardware, motor[2]._isHardware);
			log_console("err %d, %d, %d\n", mover.linearData.err[0], mover.linearData.err[1], mover.linearData.err[2]);
			log_console("errX: %d\n", mover.linearData.err[0]/(iabs(mover.linearData.size[0]) + 1));
			//log_console("Astep %d, %d, %d\n", get_steps(0), get_steps(1), get_steps(2));
			//log_console("adc %d\n", adc.value());
			send_packet_service_coords(mover.coord);
			mover.canLog = true;
		}
		else
			mover.canLog = false;
	}
}

extern "C" void NMI_Handler()
{
	RCC->CIR |= RCC_CIR_CSSC; //если сломалось тактирование от внешнего кварца
	log_console("err: external quarz has broken, use internal source %d\n", 0);//сообщим об этом и будем работать от внутреннего
}
