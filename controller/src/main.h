// инициализация и главный цикл

#ifdef STM32F407xx
#include "stm32f4xx.h"
#include "stm32f4xx_hal_conf.h"
#endif

#include "common.h"
#include "crc.h"
#include "led.h"
#include "gpio.h"
#include "fifo.h"
#include "usart.h"
#include "adc.h"
#include "stdio.h"
#include "string.h"
#include "sys_timer.h"
#include "executor.h"
#include "receiver.h"
#include "track.h"
#include "system.h"

//===============================================================
void init_motors()
{
	for (int i=0; i < MAX_HARD_AXES; ++i)
		motor[i]._index = i;
}

//===============================================================
void main_init()
{
	init_system();
	led.init();
	timer.init();
	crc.init();
	usart.init();
	receiver.init();
	tracks.init();
	configure_gpio();
	configure_timers();
	init_motors();
	adc.init();
}

//===============================================================
int timeToSend; // следующая посылка координат
int numShow;    // сколько раз подряд послать координаты
int maxLoopTime = 0; // максимальное время цикла
int prevLoopTS = 0;  // время предыдущего запуска цикла

void main_setup()
{
	main_init();
	led.show(0);
	executor.init();

	timeToSend = timer.get_ms(500); // предварительная пауза на всякий случай
	executor.canLog = true;
	numShow = 0;
}

void main_loop()
{
	int stepTS = timer.get_ticks(); // время обработки одного шага
	int loopTime = stepTS - prevLoopTS;
	prevLoopTS = stepTS;
	if (maxLoopTime < loopTime)
		maxLoopTime = loopTime;
	executor.update();
	usart.process_receive();
	int stepTime = timer.get_ticks() - stepTime;
	
	if (executor.canLog)
	{
		send_packet_service_coords(executor.taskMove.coord);
	}
	
	if(!timer.check(timeToSend))
	{
		led.flip(0);
		timeToSend = timer.get_ms(20);
		//log_console("mlt %d, %d, %d\n", stepTime, loopTime, maxLoopTime);
		maxLoopTime = 0;
		numShow += 1; // возможность посылки короткими очередями, чтобы рассмотреть детали процесса
	}
	
	if (numShow > 0)
	{
		--numShow;
		executor.canLog = true;
	}
	else
		executor.canLog = false;
}
