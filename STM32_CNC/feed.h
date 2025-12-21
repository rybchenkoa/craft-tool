//ǁ
#include "common.h"

//===============================================================
/*
Модификатор подачи. Умеет регулировать подачу в зависимости от запросов.
*/

struct FeedModifier
{
	float feedMult;             //заданная из интерфейса скорость движения

	bool useAdcMult;            //умножать подачу на значение АЦП

	bool useThrottling;         //умножать подачу на ШИМ (подача короткими рывками)
	int throttlingSize;         //процент времени, когда движение разрешено
	int throttlingPeriod;       //частота прерывания движения по осям
	int lastThrottlingUpdate;   //время с начала периода
	bool throttling;            //запрещена ли подача

	bool useFeedStable;         //подача со стабилизацией оборотов
	bool stablePause;           //регулятор замедления/ускорения
	float stableFrequency;      //целевая частота оборотов (об/тик)

	bool useFeedPerRev;         //подача со стабилизацией нагрузки на фрезу (подача на зуб)
	float feedPerRev;           //целевое значение подачи (мм/об)

	bool useFeedSync;           //подача с синхронизацией шпинделя
	float syncStep;             //шаг витка (мм/об), нужно точное значение, т.к. ошибка суммируется
	int syncAxeIndex;           //номер оси, с которой синхронизируемся
	int syncPos;                //позиция начала витка

	//=========================================================
	FeedModifier()
	{
		feedMult = 1;
		useAdcMult = false;
		useThrottling = false;
		useFeedSync = false;
	}

	//=========================================================
	void update_throttling(int time)
	{
		int delta = time - lastThrottlingUpdate;
		if (delta >= throttlingPeriod)
		{
			lastThrottlingUpdate = time;
			delta = 0;
		}
		throttling = (delta >= throttlingSize);
	}

	//=========================================================
	void modify_feed_sync(float& feed, float currentFeed, float acceleration, int* coord, float* stepLength, float* invProj)
	{
		if (spindle.freeze)
			feed = 0;
		else
		{
			int index = syncAxeIndex; //для удобства
			float spindleFeed = spindle.velocity * syncStep; // скорость движения резьбы, мм/мкс
			spindleFeed *= invProj[index] / stepLength[index]; // сравниваем полные скорости, а не проекцию на шпиндель
			spindleFeed *= float(TIMER_FREQUENCY) / CORE_FREQ; // приводим к мм/такт
			//на начальном участке просто разгоняемся
			if (currentFeed > spindleFeed * 0.8f)
			{
				//пытаемся попасть в ближайший виток
				float position = fabs(coord[index] - syncPos) * stepLength[index]; //текущий отступ от начала витков
				float fraction = position / syncStep; //пройдено витков
				fraction -= int(fraction); //пройденная часть витка
				float delta = fraction - spindle.position; //на какую часть витка отклонились
				if (delta < 0) //убираем лишний оборот:(-1, 1) в (0, 1)
					delta += 1.f;
				delta -= 0.5f; // получается сдвиг на полвитка относительно датчика
				delta *= syncStep;
				
				float deltaFeed = currentFeed - spindleFeed;
				//известны s - расстояние со знаком, a - ускорение, v - скорость
				//надо понять, пора тормозить или ускоряться
				//s = v*v/2a - тормозное расстояние
				if (delta < 0) // если отстаём
				{
					// но догоняем, и оставшееся расстояние меньше тормозного пути
					if (deltaFeed > 0 && -2 * delta * acceleration < deltaFeed * deltaFeed)
						feed = 0;
				}
				else // если опережаем
				{
					// и нас не догоняют или тормозного расстояния хватает, тогда ещё притормозим
					if (deltaFeed > 0 || 2 * delta * acceleration > deltaFeed * deltaFeed)
						feed = 0;
				}
			}
		}
	}

	//=========================================================
	void modify_stable_frequence(float& feed)
	{
		if (spindle.velocity < stableFrequency)
			feed = 0;
	}

	//=========================================================
	void modify_feed_per_rev(float& feed)
	{
		//подача в мм/тик, скорость в оборотах/тик
		float targetFeed = spindle.velocity * feedPerRev;
		
		if (feed > targetFeed)
			feed = targetFeed;
	}

	//=========================================================
	void update(int time)
	{
		if (useThrottling)
			update_throttling(time);
	}

	//=========================================================
	void modify(float& feed, float currentFeed, float acceleration, int* coord, float* stepLength, float* invProj)
	{
		feed *= feedMult;

		//различные способы модификации подачи
		//управление скоростью через напряжение
		if (useAdcMult)
			feed *= adc.value() * (1.0f/(1<<12)); //на максимальном напряжении   *= 0.99999

		//движение рывками
		if (useThrottling && throttling)
			feed = 0;

		//поддержание постоянных оборотов шпинделя
		if (useFeedStable)
			modify_stable_frequence(feed);

		//постоянная подача на зуб
		if (useFeedPerRev)
			modify_feed_per_rev(feed);

		//попадание в витки резьбы
		if (useFeedSync)
			modify_feed_sync(feed, currentFeed, acceleration, coord, stepLength, invProj);
	}

	//=========================================================
	void clear_flags()
	{
		useFeedStable = false;
		useFeedPerRev = false;
		useFeedSync = false;
	}

	//=========================================================
	void set_mode(PacketSetFeedMode *pack)
	{
		switch(pack->mode)
		{
			case FeedType_ADC:
			{
				auto p = (PacketSetFeedAdc*)pack;
				useAdcMult = p->enable;
				break;
			}
			case FeedType_THROTTLING:
			{
				auto p = (PacketSetFeedThrottling*)pack;
				useThrottling = p->enable;
				throttlingPeriod = p->period;
				throttlingSize = p->size;
				break;
			}
			//эти друг друга выключают
			case FeedType_NORMAL:
			{
				clear_flags();
				break;
			}
			case FeedType_STABLE_REV:
			{
				clear_flags();
				useFeedStable = true;
				stableFrequency = ((PacketSetFeedStable*)pack)->frequency;
				break;
			}
			case FeedType_PER_REV:
			{
				clear_flags();
				useFeedPerRev = true;
				feedPerRev = ((PacketSetFeedPerRev*)pack)->feedPerRev;
				break;
			}
			case FeedType_SYNC:
			{
				clear_flags();
				useFeedSync = true;
				syncAxeIndex = ((PacketSetFeedSync*)pack)->axeIndex;
				syncPos = ((PacketSetFeedSync*)pack)->pos;
				syncStep = ((PacketSetFeedSync*)pack)->step;
				break;
			}
		}
	}
};
