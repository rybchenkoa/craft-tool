// контроллер станка с ЧПУ для платы ESP32

#include "main.h"

void main_task(void*)
{
	disableCore0WDT();
	main_setup();
	while(true) {
		main_loop();
		yield();
	}
}

void setup()
{
	TaskHandle_t task;
	// если связь не занимает много времени, тогда её и обновление делаем на 0 ядре, а шаги генерируем на 1
	int loopCore = 1;
#ifdef STEPS_DEDICATED_CORE
	loopCore = 0;
#endif
	// размер стека, параметры, приоритет, дескриптор, ядро
	xTaskCreatePinnedToCore(main_task, "main", 4000, nullptr, tskIDLE_PRIORITY + 24, &task, loopCore);
	vTaskDelete(NULL);
}

void loop()
{
}
