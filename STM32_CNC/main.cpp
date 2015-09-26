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
	for (int i=0; i < COUNT_DRIVES; ++i)
		motor[i].index = i;
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
	configure_GPIO();
	init_motors(); //уже использует ШИМ
	adc.init();
}

//===============================================================
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
			//log_console("adc %d\n", adc.value());
			send_packet_service_coords(mover.coord);
		}
	}
}


