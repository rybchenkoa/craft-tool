//ǁ
#include "stm32f4xx.h"
#include "stm32f4xx_hal_conf.h"
#include "led.h"
#include "gpio_use.h"
#include "fifo.h"
#include "USART_module.h"
#include "adc.h"
#include "stdio.h"
#include "string.h"
#include "sys_timer.h"
#include "mover.h"
#include "system.h"

//===============================================================
void init_motors()
{
	for (int i=0; i < MAX_HARD_AXES; ++i)
		motor[i]._index = i;
}

//===============================================================
void init()
{
	init_fault_irq();
	init_system_clock();
	connect_peripherals();
	init_default_gpio();

	led.init();	
	led.hide(0);
	timer.init();
	
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
	led.show(0);

	mover.init();
	int timeToSend = timer.get_ms(500); //предварительная пауза на всякий случай
	int stepTime; //время обработки одного шага
	mover.canLog = true;
	int numShow = 0;
	while(1)
	{
		stepTime = timer.get_ticks();
		mover.update();
		usart.process_receive();
		stepTime = timer.get_ticks() - stepTime;
		
		if (mover.canLog)
		{
			send_packet_service_coords(mover.coord);
		}
		
		if(!timer.check(timeToSend))
		{
			led.flip(0);
			timeToSend = timer.get_ms(20);
			numShow +=1; //возможность посылки короткими очередями, чтобы рассмотреть детали процесса
		}
		if (numShow > 0)
		{
			--numShow;
			mover.canLog = true;
		}
		else
			mover.canLog = false;
	}
}
