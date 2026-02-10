#pragma once
// задаёт конфигурацию выходных пинов

// разводка выводов под двигатели
// STEP [32, 25, 27, 23, 21]
// DIR  [33, 26, 14, 22, 19]
// выводы ШИМ [18, 5  аппаратные 4, 15]

#include "sys_timer.h"

#define MAX_HARD_AXES 5
#define MAX_PWM 2
#define PWM_RESOLUTION 10
#define MAX_PWM_WIDTH ((1<<PWM_RESOLUTION)-1)

// 16-бит максимальное время между шагами для генерации в отдельном потоке
#define MAX_PERIOD (1<<16)
#define MAX_STEP 0 // 32-битный программный счётчик, поэтому при переполнении ничего не добавляем

int STEP_PINS[] = {32, 25, 27, 23, 21};
int DIR_PINS[]  = {33, 26, 14, 22, 19};

// входы 39, 34, 35
int IN_PINS[]  = {39, 34, 35};
int polarity = 0; // надо ли инвертировать вход (битовый массив)

int OUT_PINS[]    = {18, 5, 4, 15};

// данные для генерации шагов
struct SoftTimer
{
	int period = 0;   // время шага (половина меандра)
	int lastTime = 0; // время начала шага
	int steps = 0;    // число шагов (полуволн)
	int /*bool*/ active = false; // включен ли таймер
};

SoftTimer stepTimers[MAX_HARD_AXES];
volatile int updatesCounter;

void timers_update(void*);

//--------------------------------------------------
void configure_gpio()
{
	// входы
	for (int i = 0; i < 3; ++i) {
		pinMode(IN_PINS[i], INPUT);
	}

	// PWM
	for (int i = 0; i < MAX_SLOW_PWMS + MAX_PWM; ++i) {
		pinMode(OUT_PINS[i], OUTPUT);
	}

	// STEP / DIR управляется вручную
	for (int i = 0; i < MAX_HARD_AXES; ++i) {
		pinMode(STEP_PINS[i], OUTPUT);
		pinMode(DIR_PINS[i], OUTPUT);
	}
}

//--------------------------------------------------
//один таймер для каналов ШИМ
void config_pwm_timer()
{
	int channel = 0;
	for (int i = MAX_SLOW_PWMS; i < MAX_SLOW_PWMS + MAX_PWM; ++i)
	{
		ledcSetup(channel, PWM_FREQ, PWM_RESOLUTION); // настраиваем таймер
		ledcAttachPin(OUT_PINS[i], channel); // подключаем к ножке
		ledcWrite(channel, 0); // выдаём по умолчанию 0 на выходе
	}
}

//--------------------------------------------------
void configure_timers()
{
	config_pwm_timer();
	// запускаем update на отдельном ядре
	disableCore0WDT();
	TaskHandle_t task;
	xTaskCreatePinnedToCore(timers_update, "steps", 1000, nullptr, 1, &task, 0);
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
	stepTimers[index].period = ticks;
}

//--------------------------------------------------
//включает автошаги
void inline enable_step_timer(int index)
{
	stepTimers[index].active = true;
}

//--------------------------------------------------
//выключает автошаги
void inline disable_step_timer(int index)
{
	stepTimers[index].active = false;
	
	// убеждаемся, что не влезем посреди обновления таймера
	int count = updatesCounter;
	while (count == updatesCounter)
		;
}

//--------------------------------------------------
//возвращает число шагов, сделанных таймером
int inline get_steps(int index)
{
	return stepTimers[index].steps;
}

//--------------------------------------------------
//делает шаг таймером
void inline step(int index)
{
	SoftTimer* timer = &stepTimers[index];
	++timer->steps;
	set_pin_state(STEP_PINS[index], timer->steps & 1);
}

//--------------------------------------------------
// выставляет время до следующего шага
// применяется к неактивному таймеру
void inline set_next_step_time(int index, int time)
{
	SoftTimer* tim = &stepTimers[index];
	tim->lastTime = timer.get_ticks() - tim->period + time;
}

//--------------------------------------------------
// программная эмуляция периферии на отдельном ядре
void timers_update(void*)
{
	while(true) {
		int time = timer.get_ticks();
		for (int i = 0; i < MAX_HARD_AXES; ++i) {
			SoftTimer* tim = &stepTimers[i];
			if (tim->active) {
				// значения могут быть изменены посреди обновления, делаем транзакционно
				int lastTime = tim->lastTime;
				int period = tim->period;
				if (time - lastTime > period) {
					++tim->steps;
					set_pin_state(STEP_PINS[i], tim->steps & 1);
					tim->lastTime = lastTime + period;
				}
			}
			++updatesCounter;
		}
	}
}


//--------------------------------------------------
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
	ledcWrite(index, width * MAX_PWM_WIDTH);
}

//--------------------------------------------------
//задает периодичность импульсов на аппаратной ножке
void inline set_pwm_frequency(float frequency)
{
	for (int channel = 0; channel < MAX_PWM; ++channel)
		ledcSetup(channel, frequency, PWM_RESOLUTION);
}
