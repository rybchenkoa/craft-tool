//ǁ
#include "common.h"

//===============================================================
/*
Шпиндель. При наличии датчика умеет вычислять свою позицию и скорость.
*/

struct Spindle
{
	int pinNumber;            //номер входа, с которого получаем сигнал
	int marksCount;           //количество меток на шпинделе
	float markPositions[MAX_SPINDLE_MARKS*2]; //позиции меток
	//началом координат считается переход из 1 в 0
	//нулевой элемент = 0
	//первая координата означает расстояние от начала координат
	//до перехода из 0 в 1
	//при вращении в обратную сторону переходы инвертируются

	bool reverse;             //true - крутится в обратную сторону
	int lastIndex;            //на какой метке были в последний раз
	bool lastSensorState;     //что выдавал датчик на предыдущем такте
	bool lastTime;            //время предыдущего переключения датчика
	bool freeze;              //если долго не переключался, считаем что не крутится
	float position;           //интерполированная текущая позиция шпинделя
	float velocity;           //скорость, оборотов/мкс

	Spindle()
	{
		pinNumber = -1;
	}

	int add_index(int index)
	{
		if (!reverse)
		{
			++index;
			if (index >= marksCount)
				index = 0;
		}
		else
		{
			--index;
			if (index < 0)
				index = marksCount - 1;
		}

		return index;
	}

	float round_diff_pos(float from, float to)
	{
		float delta = reverse ? from - to : to - from;
		
		if (delta < 0)
			delta += 1.f; //следующий круг
		
		return delta;
	}

	void update(int time)
	{
		if(pinNumber == -1)
			return;
		
		bool sensorState = get_pin(pinNumber);
		if (sensorState != lastSensorState)
		{
			float prevPosition = markPositions[lastIndex];
			lastIndex = add_index(lastIndex);
			position = markPositions[lastIndex];
			
			if (!freeze)
			{
				float deltaPos = round_diff_pos(prevPosition, position);
				
				int deltaTime = time - lastTime;
				velocity = deltaPos / deltaTime;
			}
			
			lastSensorState = sensorState;
			lastTime = time;
			freeze = false;
		}
		else
		{
			int deltaTime = time - lastTime;
			if (deltaTime > SPINDLE_FREEZE_TIME)
			{
				freeze = true;
				velocity = 0.f;
			}
			
			if (!freeze)
			{
				float prevPosition = markPositions[lastIndex];
				int nextIndex = add_index(lastIndex);
				float nextPosition = markPositions[nextIndex];
				float maxDelta = round_diff_pos(prevPosition, nextPosition);
				float offset = deltaTime * velocity;
				if (maxDelta > offset)
					offset = maxDelta;
				position = prevPosition + offset;
			}
		}
	}
};

Spindle spindle;
