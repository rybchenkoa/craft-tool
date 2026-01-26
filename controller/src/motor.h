#pragma once
// массив двигателей, выдаёт сигналы управления на пинах

#include "common.h"

//===============================================================
//всё что относится к одному экземпляру двигателя
/* движку задается период шагов в тактах
он умеет вычислять свои координаты
нужно периодически его опрашивать, чтобы обработать переход через период таймера
или чтобы он шагал, если период большой
*/

struct Motor
{
	int _index;         //номер оси
	int _position;      //по каким координатам сейчас расположена гайка
	bool _direction;    //куда считать
	bool _isHardware;   //катит с помощью таймера или нет
	int _lastTime;      //время последнего шага
	//_isHardware = false
	int _period;        //текущее время между шагами
	//_isHardware = true
	int _lastCnt;       //у таймера узнаём не реальную позицию, а разницу в шагах

	//===============================================================
	Motor()
	{
		_index = 0;
		_position = 0;
		_direction = false;
	}

	//===============================================================
	void reset()
	{
		_position = 0;
	}

	//===============================================================
	bool can_step(int time)
	{
		return time - _lastTime > _period / 2;
	}
	
	//===============================================================
	void one_step(int time)
	{
		step(_index); //делаем шаг
		if (_direction) //двигаем позицию
			++_position;
		else
			--_position;
		_lastTime = time;
	}
	
	//===============================================================
	//захватывает текущее состояние шагов
	void shot(int time)
	{
		if (_period == MAX_STEP_TIME)
			return;
			
		if (_isHardware) //аппаратные шаги
		{
			int currentCnt = get_steps(_index);
			int delta = currentCnt - _lastCnt; //находим разницу с предыдущего раза

			if (delta < 0)
				delta += MAX_STEP;
			if (_direction)
				_position += delta;
			else
				_position -= delta; //учитываем её
			_lastCnt = currentCnt;
			if (delta != 0)
				_lastTime = time; //запоминаем время последнего шага
		}
		else
		{
			int delta = time - _lastTime;
			if (delta >= _period) //если с предыдущего шага прошел период
			{
				step(_index); //делаем шаг
				if (_direction) //двигаем позицию
					++_position;
				else
					--_position;
				_lastTime += _period; //запоминаем время шага (так должна получиться стабильная частота программных шагов)
				if (time - _lastTime > _period/2) //чтобы не получилось, что при резком уменьшении времени шага сделало несколько шагов
					_lastTime = time;
			}
		}
	}

	//===============================================================
	//изменяет период
	void set_period(int period)
	{
		if (_isHardware)
		{
			if (period >= MAX_PERIOD) //переключаем на программный счёт
			{
				disable_step_timer(_index);
				shot(timer.get_ticks());
				_isHardware = false;
			}
			else
				set_step_time(_index, period);
		}
		else
		{
			if (period < MAX_PERIOD) //переключаем на аппаратный счёт
			{
				set_step_time(_index, period);
				int timeLeft = _lastTime + _period - timer.get_ticks();
				if (timeLeft < 0)
					timeLeft = 0;
				set_next_step_time(_index, timeLeft);
				_lastCnt = get_steps(_index);
				enable_step_timer(_index);
				_isHardware = true;
			}
			else //масштабируем прошедшее время для более точного учета шагов
			{
				int curTime = timer.get_ticks();
				if (_period == MAX_STEP_TIME)
				{
					if (period < MAX_STEP_TIME) //при включении мотора заново инициализируем время шага
						_lastTime = curTime;
				}
				else
				{
					int timeLeft = _lastTime + _period - curTime;
					//timeLeft = timeLeft * period / _period, чтобы не было потери точности, это выражение переписано в другом виде
					timeLeft += int(timeLeft * float(period - _period) / _period);
					_lastTime = curTime - period + timeLeft;
				}
			}
		}
		_period = period;
	}

	//===============================================================
	void start(int period)
	{
		_isHardware = false;
		_lastTime = timer.get_ticks();
		_period = period;
	}

	//===============================================================
	void stop()
	{
		if (_isHardware)
		{
			disable_step_timer(_index);
			shot(timer.get_ticks());
			_isHardware = false;
		}
	}

	//===============================================================
	void set_direction(bool direction)
	{
		_direction = direction;
		set_dir(_index, direction);
	}
};

Motor motor[MAX_HARD_AXES];


//===============================================================
struct VirtualMotor
{
	int _position;
	int _period;
	int _newPeriod; //таймера не умеют на лету менять значение и на старте есть задержка (имитируем их работу)
	int _lastTime;
	
	VirtualMotor() { _position = 0; }

	//===============================================================
	void shot(int time)
	{
		if (_period == MAX_STEP_TIME)
			return;

		int delta = (time - _lastTime) / _period;
		_position -= delta;
		_lastTime += delta * _period;
		if (delta > 0)
			_period = _newPeriod;
	}
	
	//===============================================================
	void set_period(int period)
	{
		int curTime = timer.get_ticks();
		if (_period == MAX_STEP_TIME)
		{
			_lastTime = curTime;
			_period = period;
		}
		else if (_period >= MAX_PERIOD)
		{
			if (_newPeriod != MAX_STEP_TIME)
			{
				_period = _newPeriod;
				int timeLeft = _lastTime + _period - curTime;
				timeLeft += int(timeLeft * float(period - _period) / _period);
				_lastTime = curTime - period + timeLeft;
			}
			_period = period;
			_newPeriod = period;
		}
		else
		{
			_newPeriod = period;
		}
	}
	
	//===============================================================
	void start(int period)
	{
		_lastTime = timer.get_ticks();
		_period = period;
		_newPeriod = _period;
	}
};

VirtualMotor virtualAxe;
