//#include "math.h"
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
	//захватывает текущее состояние шагов
	void shot(int time)
	{
		if (_isHardware) //аппаратные шаги
		{
			int currentCnt = get_steps(_index);
			int delta = currentCnt - _lastCnt; //находим разницу с предыдущего раза
//log_console("delta %d, dir %d\n", delta, _direction);
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
				//_lastTime += _period; //запоминаем время шага
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
				shot(timer.get());
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
				set_next_step_time(_index, _lastTime + _period - timer.get());
				_lastCnt = get_steps(_index);
				enable_step_timer(_index);
				_isHardware = true;
			}
		}
		_period = period;
	}

	//===============================================================
	void start(int period)
	{
		_isHardware = false;
		_lastTime = timer.get();
		_period = period;
	}

	//===============================================================
	void stop()
	{
		if (_isHardware)
		{
			disable_step_timer(_index);
			shot(timer.get());
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

Motor motor[MAX_AXES];
