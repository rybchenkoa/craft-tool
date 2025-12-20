//ǁ
#pragma once
#include "stdint.h"
//#include <stdio.h>
//#include <stdarg.h>

#define M_PI 3.1415f

const int MAX_AXES = 5;
const int CORE_FREQ = 168000000;
const int PWM_SIZE = CORE_FREQ / 20000;    //дефолтные частоты
const int PWM_SLOW_SIZE = 1000000 / 1000;//
const int MAX_SLOW_PWMS = 3;
const int MAX_STEP_TIME = 1<<30;
const int MAX_SPINDLE_MARKS = 10; //10 меток, 20 переключений туда/сюда
const int SPINDLE_FREEZE_TIME = 1000000; //время между метками, после которого считаем, что шпиндель не крутится
const int TIMER_FREQUENCY = 1000000; // частота системного таймера

//#define log_console(format, ...) {}

#define log_console(format, ...) \
{\
	char buffer[128];\
	buffer[0] = DeviceCommand_TEXT_MESSAGE;\
	int count = sprintf(buffer+1, format, __VA_ARGS__);\
	*(int*)(buffer + count + 2) = calc_crc(buffer, count + 2);\
	send_packet(buffer, count + 2 + 4);\
}

/*
void log_console(char *format, ...)
{
	char buffer[128];
	va_list args;
	va_start (args, format);
	buffer[0] = DeviceCommand_TEXT_MESSAGE;
	int count = vsprintf (buffer+1, format, args);
	*(int*)(buffer + count + 2) = calc_crc(buffer, count + 2);
	send_packet(buffer, count + 2 + 4);
	va_end (args);
}
*/
