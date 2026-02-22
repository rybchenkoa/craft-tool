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
	// размер стека, параметры, приоритет, дескриптор, ядро
	xTaskCreatePinnedToCore(main_task, "main", 4000, nullptr, tskIDLE_PRIORITY+24, &task, 0);
	vTaskDelete(NULL);
}

void loop()
{
}
