#pragma once
// шагает и вычисляет отклонение двигателей от необходимой позиции

#include "motor.h"
#include "sys_timer.h"


struct Discretization
{
	int refSize;           // исходный размер опорной оси
	int size[AXES_COUNT];    // размеры линии
	int err[AXES_COUNT];     // ошибка координат (внутреннее представление)
	int sign[AXES_COUNT];    // изменение координаты при превышении ошибки {-1 || 1}
	int last[AXES_COUNT+1];  // последние координаты, для которых была посчитана ошибка
	int from[AXES_COUNT];    // откуда двигаемся
	int to[AXES_COUNT];      // куда двигаемся

	Discretization()
	{
		for (int i = 0; i < AXES_COUNT; ++i) {
			to[i] = 0;
		}
	}
	
	//---------------------------------------------------------------------
	void compute_error()
	{
		// допустим, размеры по x, y = a, b  , отрезок начинается с 0
		// тогда уравнение будет a*y = b*x;   a*dy = b*dx;  a*dy - b*dx = 0
		// ошибка по y относительно x при смещении:  (a*dy - b*dx) / a
		int dx = -(virtualAxe._position - last[AXES_COUNT]); // находим изменение опорной координаты
		last[AXES_COUNT] = virtualAxe._position;
		for (int i = 0; i < AXES_COUNT; ++i)
		{
			int dy = (motor[i]._position - last[i]) * sign[i]; // находим изменение текущей координаты
			int derr = dy * refSize - dx * size[i]; // находим ошибку текущей координаты
			err[i] += derr;
			last[i] = motor[i]._position;
		}
	}

	//---------------------------------------------------------------------
	void get_normalized_error(float error[AXES_COUNT])
	{
		float errCoef = 1.0f / refSize;
		for (int i = 0; i < AXES_COUNT; ++i) {
			error[i] = err[i] * errCoef;
		}
	}

	//---------------------------------------------------------------------
	void update_position(int time)
	{
		// быстро запоминаем текущее состояние координат
		for (int i = 0; i < AXES_COUNT; ++i)
			if (size[i] != 0 && motor[i]._isHardware)
				motor[i].shot(time);
		virtualAxe.shot(time); // виртуальная ось самая большая, если она = 0, то выйдет еще при инициализации

		// находим ошибку новых координат
		compute_error();

		// медленными осями шагаем точно
		for (int i = 0; i < AXES_COUNT; ++i) {
			if (size[i] != 0 && !motor[i]._isHardware)
			{
				if (motor[i].can_step(time) && err[i] < -refSize / 2)
				{
					motor[i].one_step(time);
					err[i] += refSize;
					last[i] += sign[i];
				}
			}
		}
	}

	//---------------------------------------------------------------------
	bool init_segment(int coord[AXES_COUNT], int dest[AXES_COUNT], int ref, bool homing)
	{
		bool diff = false;
		for(int i = 0; i < AXES_COUNT; ++i)
		{
			from[i] = to[i];       // куда должны были доехать

			if (!homing)
				to[i] = dest[i];   // куда двигаемся
			else
				to[i] += dest[i];  // инкрементальный режим для хоминга, так как текущие координаты на компьютере могут быть сбиты

			if(to[i] > from[i])
			{
				size[i] = to[i] - from[i]; // увеличение ошибки
				sign[i] = 1;               // изменение координаты
			}
			else
			{
				size[i] = from[i] - to[i];
				sign[i] = -1;
			}

			if(to[i] != from[i])
				diff = true;
		}

		// размер опорной оси запоминаем исходный, а координату считаем от текущих координат
		// чтобы потом при расчёте ошибки правильно скорректировать ошибку текущей позиции осей
		// потому что в предыдущем отрезке аппаратные оси могли проехать за его конец
		refSize = size[ref];
		virtualAxe._position = (to[ref] - coord[ref]) * sign[ref];

		// инициализируем ошибку, учитывая, что в начале отрезка она = 0
		for(int i = 0; i < AXES_COUNT; ++i)
		{
			err[i] = 0;
			last[i] = from[i];
		}
		last[AXES_COUNT] = refSize;

		// пересчитываем ошибку от начала отрезка до текущих координат
		compute_error();

		return diff;
	}

	//---------------------------------------------------------------------
	void finish(int coord[AXES_COUNT])
	{
		for(int i = 0; i < AXES_COUNT; ++i)
			to[i] = coord[i];
	}
};
