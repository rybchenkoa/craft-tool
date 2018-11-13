//ǁ
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
	led.hide(0);
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
	led.show(0);

	mover.init();
	int timeToSend = timer.get_ms(500); //предварительная пауза на всякий случай
	int stepTime; //время обработки одного шага
	mover.canLog = true;
	int numShow = 0;
	while(1)
	{
		stepTime = timer.get();
		mover.update();
		stepTime = timer.get() - stepTime;
		
		if (mover.canLog)
		{
			//log_console("dest %d %d %d, sc %d\n", mover.to[0], mover.to[1], mover.to[2]);
		//	log_console("stepT %d, st %d, tmr %d\n", stepTime, mover.linearData.state, timer.get());
		//	log_console("queue %d, ulen %d, trcnt %d\n", receiver.queue.Count(), receiver.tracks.Front().segments, receiver.tracks.Count());
			//log_console("mot %d, %d, %d\n", motor[0]._isHardware, motor[1]._isHardware, motor[2]._isHardware);
			//log_console("err %d, %d, %d\n", mover.linearData.err[0], mover.linearData.err[1], mover.linearData.err[2]);
		//	log_console("ervr: %d, %d, %d\n", mover.linearData.error[0], mover.linearData.error[1], mover.linearData.error[2]);
		//	log_console("coord: %d, %d, %d\n", mover.coord[0], mover.coord[1], mover.coord[2]);
	//		log_console("coord %d, ref %d\n", mover.coord[0], virtualAxe._position);
	//		log_console("step %d, ref %d\n", motor[0]._period, virtualAxe._period);
			//log_console("Astep %d, %d, %d\n", get_steps(0), get_steps(1), get_steps(2));
			//log_console("adc %d\n", adc.value());
	//		int coord[MAX_AXES];
	//		coord[0] = virtualAxe._position;
	//		coord[1] = mover.coord[0];
	//		coord[2] = 0;
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

extern "C" void NMI_Handler()
{
	RCC->CIR |= RCC_CIR_CSSC; //если сломалось тактирование от внешнего кварца
	log_console("err: external quarz has broken, use internal source %d\n", 0);//сообщим об этом и будем работать от внутреннего
}
