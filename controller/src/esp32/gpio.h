#pragma once
//задаёт конфигурацию выходных пинов

#include "common.h"


//=====================================================================
//разводка выводов под двигатели
//STEP [d12, d13, d14, d15, g4]
//DIR [d10, d11, g2, g3, g6]
//выводы ШИМ [e8, e10, e12  аппаратные e9, e11, e13, e14]

#define MAX_HARD_AXES 5
#define MAX_PWM  4

//16-бит максимальное время между шагами для аппаратного таймера
//тактирование на 1/2 частоты процессора
//время периода = ARR+1, поэтому здесь можно вылезть на 1 за пределы 16 бит
#define MAX_PERIOD (1<<17)
#define MAX_STEP 256 //8-битный счетчик шагов, 0 пропускается, вместо него 0x1 00

int STEP_PINS[] = { 1,  2,  3,  4,  5};
int DIR_PINS[]  = { 1,  2,  3,  4,  5};

//входы c11, c12, d0, d1, d2, d3, d4, d5
int IN_PINS[]  = { 1,  2,  3,  4,  5};
int polarity = 0; //надо ли инвертировать вход (битовый массив)

int OUT_PINS[]    = {8, 10, 12, 9, 11, 13, 14};

//--------------------------------------------------
void configure_gpio()
{
	// входы
	for (int i = 0; i < 8; ++i) {
		pinMode(IN_PINS[i], INPUT);
	}

	// PWM
	for (int i = 0; i < MAX_SLOW_PWMS + MAX_PWM; ++i) {
		pinMode(OUT_PINS[i], OUTPUT);
	}

	// DIR управляется вручную
	for (int i = 0; i < MAX_HARD_AXES; ++i) {
		pinMode(STEP_PINS[i], OUTPUT);
		pinMode(DIR_PINS[i], OUTPUT);
	}
}

//--------------------------------------------------
//один таймер для каналов ШИМ
void config_pwm_timer()
{
}

//пересчитывает такты процессора в такты таймера
int inline to_tim_clock(int interval) { return interval >> 1; }

//--------------------------------------------------
void configure_timers()
{
	config_pwm_timer();
}


//--------------------------------------------------
//--------------------------------------------------
//задаёт состояние пина порта
void inline set_pin_state(int pinNum, bool state)
{
	digitalWrite(pinNum, state ? HIGH : LOW);
}

//--------------------------------------------------
//задает направление
void inline set_dir(int index, bool state)
{
	set_pin_state(DIR_PINS[index], state);
}

//--------------------------------------------------
//получает сигнал на ножке
bool inline get_pin(int index)
{
	int pinNum = IN_PINS[index];
	return ((digitalRead(pinNum) == HIGH ? 1 : 0) ^ (polarity >> index)) & 1;
}

//--------------------------------------------------
//задает время между шагами
void inline set_step_time(int index, int ticks)
{
}

//--------------------------------------------------
//включает автошаги
void inline enable_step_timer(int index)
{
}

//--------------------------------------------------
//выключает автошаги
void inline disable_step_timer(int index)
{
}

//--------------------------------------------------
//возвращает число шагов, сделанных таймером
int inline get_steps(int index)
{
	return 0;
}

//--------------------------------------------------
//делает шаг таймером. (делает cnt!=ccr1, а потом равным (одновременно запуская dma)
void inline step(int index)
{
}

//--------------------------------------------------
//выставляет время до следующего шага
void inline set_next_step_time(int index, int time)
{
}

//--------------------------------------------------
//выдает сигнал на ножке
void inline set_pwm_pin(int index, bool state)
{
	set_pin_state(OUT_PINS[index], state);
}

//--------------------------------------------------
//задает ширину импульсов на аппаратной ножке
void inline set_pwm_width(int index, float width)
{
}

//--------------------------------------------------
//задает периодичность импульсов на аппаратной ножке
void inline set_pwm_period(int period)
{
}
