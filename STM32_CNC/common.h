#pragma once
#include "stdint.h"

//#define USE_ADC_FEED                    //регулировка подачи напряжением

#define M_PI 3.1415f

const int NUM_COORDS = 3;
const int PWM_SIZE = 24000000 / 30000;
const int MAX_STEP_TIME = 1<<30;

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
  char buffer[256];
  va_list args;
  va_start (args, format);
  int count = vsprintf (buffer, format, args);
  send_packet(buffer, count);
  va_end (args);
}
*/
