//ǁ
#include "common.h"

//===============================================================
/*
	Шпиндель. При наличии датчика умеет вычислять свою позицию и скорость.
	Началом координат считается переход из длинного 1 в длинный 0 (должны идти подряд).
	При вращении в обратную сторону переход из длинного 0 в длинный 1.
	Каждый раз после превышения минимальной частоты после стабилизации оборотов
	происходит автоматическое измерение длин меток.
*/

struct Spindle
{
	// данные конфигурации
	int pinNumber;            //номер входа, с которого получаем сигнал
	int marksCount;           //количество меток на шпинделе, белые и черные суммарно
	float markPositions[MAX_SPINDLE_MARKS*2]; // позиции концов меток, например [0.3, 0.4, 0.5, 0.6, 0.7, 1.0]

	// измерение координаты
	int lastIndex;            //на какой метке были в последний раз
	bool lastSensorState;     //что выдавал датчик на предыдущем такте
	int sensorFilterCounter;  //защита от дребезга, сколько раз подряд повторились новые показания
	int sensorFilterSize;     //сколько отсчётов берёт фильтр
	int lastTime;             //время предыдущего переключения датчика
	bool freeze;              //если долго не переключался, считаем что не крутится
	float position;           //интерполированная текущая позиция шпинделя
	float velocity;           //скорость, оборотов/мкс

	// автокалибровка
	enum Sync
	{
		None = 0,
		InProgress,
		Done
	};

	int previousTurnPeriod; // за сколько сделали предыдущий оборот
	int currentTurnPeriod;  // сколько идёт текущий оборот
	int maxSyncPeriod;      // начиная с какой частоты можно пытаться синхронизировать
	int syncMarks[MAX_SPINDLE_MARKS*2]; // время между метками при измерении
	Sync syncState;         // на каком этапе калибровка


	Spindle()
	{
		pinNumber = -1;
		syncState = Sync::None;
		sensorFilterSize = 5;
		lastIndex = 0;
	}

	int next_index(int index)
	{
		++index;
		if (index >= marksCount)
			index = 0;

		return index;
	}

	float round_diff_pos(float from, float to)
	{
		float delta = to - from;
		
		if (delta < 0)
			delta += 1.f; //следующий круг
		
		return delta;
	}

	bool get_filtered_sensor_state()
	{
		bool state = get_pin(pinNumber);
		if (state != lastSensorState)
		{
			sensorFilterCounter++;
			if (sensorFilterCounter >= sensorFilterSize)
			{
				sensorFilterCounter = 0;
				return state;
			}
		}
		else
		{
			sensorFilterCounter = 0;
		}

		return lastSensorState;
	}

	void update(int time)
	{
		if(pinNumber == -1)
			return;
		
		int deltaTime = time - lastTime;
		bool sensorState = get_filtered_sensor_state();

		if (sensorState != lastSensorState)
		{
			// если метка пройдена, знаем позицию точно
			float prevPosition = markPositions[lastIndex];
			lastIndex = next_index(lastIndex);
			position = markPositions[lastIndex];

			if (!freeze)
			{
				float deltaPos = round_diff_pos(prevPosition, position);
				velocity = deltaPos / deltaTime;
			}
			
			lastSensorState = sensorState;
			lastTime = time;
			freeze = false;
			auto_calibrate(deltaTime);
		}
		else
		{
			// если метка не пройдена, интерполируем
			if (deltaTime > SPINDLE_FREEZE_TIME)
			{
				freeze = true;
				velocity = 0.f;
			}
			
			if (!freeze)
			{
				float prevPosition = markPositions[lastIndex];
				int nextIndex = next_index(lastIndex);
				float nextPosition = markPositions[nextIndex];
				float maxDelta = round_diff_pos(prevPosition, nextPosition);
				float offset = deltaTime * velocity;
				if (maxDelta < offset)
					offset = maxDelta;
				position = prevPosition + offset;
				if (position > 1.f)
					position -= 1.f;
			}
		}
	}

	// при необходимости обновляет размеры меток
	void auto_calibrate(int timeDelta)
	{
		syncMarks[lastIndex] = timeDelta; // в процессе синхронизации запоминаем длины меток
		currentTurnPeriod += timeDelta; // считаем время одного оборота

		// измерение периода запускаем при переходе через 0
		if (lastIndex == 0)
		{
			if (syncState == Sync::None)
			{
				// если не синхронизировано, начинаем сравнивать временные интервалы
				syncState = Sync::InProgress;
				previousTurnPeriod = 0;
			}
			else if (syncState == Sync::InProgress)
			{
				// если разогнались достаточно
				if (currentTurnPeriod < maxSyncPeriod)
				{
					float precision = 0.01f; //время витка может отличаться на 1%
					float ptp1 = previousTurnPeriod * (1 - precision);
					float ptp2 = previousTurnPeriod * (1 + precision);

					// если обороты достаточно стабилизировались, запоминаем новые длины меток
					if (ptp1 < currentTurnPeriod && currentTurnPeriod < ptp2)
					{
						syncState = Sync::Done;

						// находим максимальную метку
						int index1, index2;
						find_max_marks(index1, index2);
						// переводим временные отрезки в координаты меток в диапазоне (0..1]
						fill_with_offset(index2, currentTurnPeriod);
						lastIndex = index2 == 0 ? 0 : marksCount - index2;
					}
				}

				previousTurnPeriod = currentTurnPeriod;
			}
			else
			{
				// если уже синхронизированы, проверяем, чтобы частота не упала
				if (currentTurnPeriod > maxSyncPeriod)
				{
					syncState = Sync::None;
				}
			}

			currentTurnPeriod = 0;
		}
	}

	// в массиве измеренных длительностей меток
	// находит две соседних максимальных
	void find_max_marks(int& index1, int& index2)
	{
		int maxTime = syncMarks[0];
		int maxIndex = 0;
		for (int i = 1; i < marksCount; ++i)
		{
			if (maxTime < syncMarks[i])
			{
				maxTime = syncMarks[i];
				maxIndex = i;
			}
		}

		// находим вторую соседнюю максимальную метку
		int leftIndex = maxIndex == 0 ? marksCount - 1 : maxIndex - 1;
		int rightIndex = next_index(maxIndex);

		if (syncMarks[leftIndex] > syncMarks[rightIndex])
		{
			index1 = leftIndex;
			index2 = maxIndex;
		}
		else
		{
			index1 = maxIndex;
			index2 = rightIndex;
		}
	}

	// переводит измеренные длительности в координаты меток
	void fill_with_offset(int offset, int fullLength)
	{
		float factor = 1.f / fullLength;
		int length = 0;
		int j = offset;
		for (int i = 0; i < marksCount; ++i, ++j)
		{
			if (j == marksCount)
				j = 0;
			length += syncMarks[j];
			markPositions[i] = length * factor;
		}
	}
};

Spindle spindle;
