#include "stdint.h"

#define DIV_BITS_COUNT  5
const int DIV_BITS  =   DIV_BITS_COUNT; //число битов, отведённых на "дробную часть" шага
const int SUB_STEPS =   1<<DIV_BITS;

const int COS_AMPLITUDE   = 127;
const int COS_TABLE_COUNT = SUB_STEPS*4;

const int COUNT_DRIVES = 4; //количество двигателей

#define M_PI 3.1415f
