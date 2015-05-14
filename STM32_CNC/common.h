#pragma once
#include "stdint.h"

#define DIV_BITS_COUNT  2
const int DIV_BITS  =   DIV_BITS_COUNT; //число битов, отведённых на "дробную часть" шага
const int SUB_STEPS =   1<<DIV_BITS;    //число микрошагов в одном шаге

const int COS_AMPLITUDE   = 127;
const int COS_TABLE_COUNT = SUB_STEPS*4;

const int COUNT_DRIVES = 4; //количество двигателей

#define M_PI 3.1415f

static const int APB_FREQ = 24000000 ;        //24000/100 = 240
static const int PWM_FREQ = 100000;
static const int PWM_SIZE = APB_FREQ/PWM_FREQ-1;           //число импульсов на 1 период шим

static const int PWM_PRESCALER = 1;//APB_FREQ / (PWM_FREQ * (PWM_SIZE+1))-1;
static const uint32_t maxDelay = 1<<30;

const int NUM_COORDS = 3;
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
