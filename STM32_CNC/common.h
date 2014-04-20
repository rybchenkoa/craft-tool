#pragma once

#include "stdint.h"

#define DIV_BITS_COUNT  5
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
