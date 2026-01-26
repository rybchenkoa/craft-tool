#pragma once
// команда линейного движения

#include <cmath>

#include "task.h"
#include "track.h"
#include "packets.h"
#include "motor.h"
#include "discretization.h"
#include "sys_timer.h"
#include "feed.h"
#include "inertial.h"
#include "limit_switch.h"


// расчёт ускорения не учитывает центробежное ускорение a = v^2/r
// поэтому для малых окружностей не стоит задавать слишком большую подачу
// для v = 1000 мм/мин и r = 5 мм ускорение будет a = 56 мм/сек^2

// диапазон скорости:
// 1 шаг / 1000 сек - ограничено примерно int_max * timer resolution (1ккк * 1мкс)
// 1 000 000 шаг/сек - ограничено настройками пинов


class TaskMove : public Task
{
public:
	int coord[MAX_AXES];              // текущие координаты
	float maxVelocity[MAX_AXES];      // мм/мкс
	float maxAcceleration[MAX_AXES];  // мм/мкс^2
	float stepLength[MAX_AXES];       // мм/шаг

	MoveMode interpolation;           // режим движения
	bool needPause;                   // сбросить скорость до 0
	FeedModifier feedModifier;        // управление подачей
	Inertial inertial;                // данные для корректного изменения скорости
	LimitSwitchController switches;   // хард лимиты и хоминг

	// управление скоростью моторов для попадания на ожидаемую текущую позицию
	struct LinearData
	{
		bool enabled;            // моторы включены
		float error[MAX_AXES];   // ошибка координат
		float velCoef[MAX_AXES]; // на что умножить скорость, чтобы получить число тактов на шаг, мм/шаг
		float invProj;			 // (длина опорной координаты / полную длину)
	};

	Discretization discretization; // вычисление позиции двигателей
	LinearData linearData;

	
	//---------------------------------------------------------------------
	TaskMove()
	{
		for(int i = 0; i < MAX_AXES; i++) {
			coord[i] = 0;
		}

		needPause = false;
		interpolation = MoveMode_LINEAR;
		feedModifier = FeedModifier();
		switches = LimitSwitchController();
	}

	//---------------------------------------------------------------------
	void start_motors()
	{
		if (!linearData.enabled)
		{
			for (int i = 0; i < MAX_AXES; ++i)
				motor[i].start(MAX_STEP_TIME);
			virtualAxe.start(MAX_STEP_TIME);
			linearData.enabled = true;
		}
	}

	//---------------------------------------------------------------------
	void stop_motors()
	{
		if (linearData.enabled)
		{
			for (int i = 0; i < MAX_AXES; ++i)
				motor[i].stop();
			linearData.enabled = false;
		}
	}
	
	//---------------------------------------------------------------------
	/*
		один апдейт занимает T=3000 тактов в дебаге
		время шага t=1000, ошибка e=1
		надо сделать лишний шаг за эти 3000 сек
		для этого изменим время шага
		число шагов s=T/t между апдейтами
		новое число шагов s1=T/t-e
		t1=T/s1=T/(T/t-e)=Tt/(T-t*e)=t/(1-t*e/T)~=t*(1+t*e/T)
	*/
	void set_velocity()
	{
		// задаем скорости
		if (inertial.velocity == 0)
		{
			for (int i = 0; i < MAX_AXES; ++i)
				motor[i].set_period(MAX_STEP_TIME);
			virtualAxe.set_period(MAX_STEP_TIME);
		}
		else
		{
			int curTime = timer.get_ticks();
			int stepTimeArr[MAX_AXES+1];
			switches.reset_home(true);
			for (int i = 0; i < MAX_AXES; ++i)
			{
				if (linearData.velCoef[i] == 0)
				{
					stepTimeArr[i] = MAX_STEP_TIME;
					continue;
				}

				int stepTime;
				float axeVelocity;
				// при хоминге происходит торможение по той оси, которая наехала на свой концевик
				// при обычном движении скорость синхронно меняется у всех осей
				if (interpolation == MoveMode_HOME &&
					switches.home_braking(axeVelocity, i, curTime, inertial.velocity, linearData.velCoef[i], maxAcceleration[i]))
				{
					stepTime = 1 / axeVelocity;
				}
				else {
					float step = linearData.velCoef[i] / inertial.velocity;
					float err = linearData.error[i];
					float add = step * step * err / 1024; // t*(1+t*e/T)
					float maxAdd = step * 0.1f; // регулировка скорости максимально на 10 %
					if (std::abs(add) > maxAdd)
						add = std::copysign(maxAdd, add);
					stepTime = step + add;
				}
				
				if (stepTime > MAX_STEP_TIME || stepTime <= 0) // 0 возможен при переполнении разрядов
					stepTime = MAX_STEP_TIME;
				stepTimeArr[i] = stepTime;
			}
			// для виртуальной оси тоже считаем скорость
			{
				int stepTime = linearData.invProj / inertial.velocity;
				if (stepTime > MAX_STEP_TIME || stepTime <= 0) // 0 возможен при переполнении разрядов
					stepTime = MAX_STEP_TIME;
				stepTimeArr[MAX_AXES] = stepTime;
			}
			
			for (int i = 0; i < MAX_AXES; ++i)
			{
				motor[i].set_period(stepTimeArr[i]);
			}
			virtualAxe.set_period(stepTimeArr[MAX_AXES]);
		}
	}

	//---------------------------------------------------------------------
	bool update_motors()
	{
		int time = timer.get_ticks();

		// обновляем позиции моторов
		discretization.update_position(time);

		// вычисляем ошибку позиций
		discretization.get_normalized_error(linearData.error);
		
		for (int i = 0; i < MAX_AXES; ++i)
			coord[i] = motor[i]._position;

		// обновляем скорости двигателей
		set_velocity();
		
		if(virtualAxe._position <= 0) // если дошли до конца, выходим
			return false;
			
		return true;
	}
	
	//---------------------------------------------------------------------
	void finish()
	{
		stop_motors();
		discretization.finish(coord);
		inertial.stop();
	}

	//---------------------------------------------------------------------
	OperateResult update(bool needBreak) // override
	{
		if (switches.switch_reached(interpolation == MoveMode_HOME, discretization.size))
		{
			finish(); //TODO сделать обработку остановки
			return END;
		}
		
		if (switches.home_reached() && interpolation == MoveMode_HOME)
		{
			finish();
			return END;
		}

		int time = timer.get();
		feedModifier.update(time);

		float length = linearData.invProj * virtualAxe._position;
		length += 0.001f * tracks.current_length();

		float targetFeed = inertial.maxFeedVelocity;
		if (interpolation == MoveMode_LINEAR)
			feedModifier.modify(targetFeed, inertial.velocity, inertial.acceleration, coord, stepLength, linearData.velCoef);

		inertial.update_velocity(needPause ? 0 : targetFeed, length);

		if (!update_motors())
		{
			if (tracks.current_length() == 0)
			{
				stop_motors();
				inertial.stop();
			}
			return END;
		}
		
		// прерывание обработки
		if (needBreak && inertial.velocity == 0)
		{
			finish();
			return END;
		}

		return WAIT;
	}

	//---------------------------------------------------------------------
	bool init(int dest[MAX_AXES], int refCoord, float acceleration, int uLength, float length, float velocity)
	{
		if (tracks.decrement(uLength)) {
			inertial.stop();
		}

		bool homing = (interpolation == MoveMode_HOME);

		// если двигаться никуда не надо, то выйдет на первом такте
		if(!discretization.init_segment(coord, dest, refCoord, homing))
		{
			log_console("ERR: brez %d, %d, %d\n", dest[0], dest[1], dest[2]);
			return false;
		}

		start_motors();

		inertial.set_max_params(velocity, acceleration);
		switches.reset_home(false);
		switches.reset_active();

		for (int i = 0; i < MAX_AXES; ++i)
		{
			motor[i].set_direction(discretization.sign[i] > 0);
			
			if (discretization.size[i] == 0)
				linearData.velCoef[i] = 0;
			else
			{
				linearData.velCoef[i] = length / discretization.size[i];
				
				switches.activate_switch(i, homing, discretization.sign[i] > 0);
			}
		}
		
		linearData.invProj = linearData.velCoef[refCoord];
		
		return true;
	}

	//---------------------------------------------------------------------
	void process_packet_set_vel_acc(PacketSetVelAcc *packet)
	{
		float mks = 0.000001;
		for(int i = 0; i < MAX_AXES; ++i)
		{
			maxVelocity[i] = packet->maxVelocity[i] * mks;
			maxAcceleration[i] = packet->maxAcceleration[i] * mks * mks;
			log_console("[%d]: maxVel %d, maxAcc %d\n", i, int(packet->maxVelocity[i]), int(packet->maxAcceleration[i]));
		}
	}

	//---------------------------------------------------------------------
	void process_packet_set_coords(PacketSetCoords *packet)
	{
		for (int i = 0; i < MAX_AXES; ++i) {
			if (packet->used | (1 << i))
			{
				coord[i] = packet->coord[i];
				motor[i]._position = coord[i];
				discretization.to[i] = coord[i];
			}
		}
	}

	//---------------------------------------------------------------------
	void process_packet_set_step_size(PacketSetStepSize *packet)
	{
		for(int i = 0; i < MAX_AXES; ++i)
		{
			stepLength[i] = packet->stepSize[i];
			float mk = stepLength[i]*1000;
			int z = int(mk);
			log_console("[%d]: stepLength %d.%02d um\n", i, z, int(mk*100 - z*100));
		}
	}
};
