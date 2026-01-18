#pragma once
// обработка концевиков

#include "packets.h"
#include "gpio.h"


class LimitSwitchController
{
public:
	// данные хард лимитов
	char limitSwitchMin[MAX_AXES]; // концевики
	char limitSwitchMax[MAX_AXES];
	char activeSwitch[MAX_AXES];   // концевики, в сторону которых сейчас едем
	int  activeSwitchCount;        // количество активных концевиков

	// данные хоминга
	char homeSwitch[MAX_AXES];     // датчики дома
	float axeVelocity[MAX_AXES];   // скорость оси во время попадания в концевик
	int  timeActivate[MAX_AXES];   // время начала остановки
	bool homeActivated[MAX_AXES];  // при наезде на дом выставляем бит
	bool homeReached;

	
	//---------------------------------------------------------------------
	LimitSwitchController()
	{
		for (int i = 0; i < MAX_AXES; i++)
		{
			limitSwitchMin[i] = -1;
			limitSwitchMax[i] = -1;
			homeSwitch[i] = -1;
		}
	}

	//---------------------------------------------------------------------
	// очистка активных концевиков
	void reset_active()
	{
		activeSwitchCount = 0;
	}

	//---------------------------------------------------------------------
	// активация концевиков в заданном направлении
	void activate_switch(int i, bool homing, bool direction)
	{
		int lim = direction ? limitSwitchMax[i] : limitSwitchMin[i];
		if (homing)
				activeSwitch[i] = lim; // при движении домой торможение раздельное
		else
			if (lim != -1)
				activeSwitch[activeSwitchCount++] = lim; // иначе нас интересует любое срабатывание
		homeActivated[i] = false;
	}

	//---------------------------------------------------------------------
	// наезд на хард лимит
	bool switch_reached(bool homing, int* size)
	{
		// при обычной езде при налете на концевик прекращаем выдавать STEP
		if (!homing)
		{
			for (int i = 0; i < activeSwitchCount; ++i)
				if (activeSwitch[i] != -1 && get_pin(activeSwitch[i]))
				{
					log_console("switch %d, %d reached\n", i, activeSwitch[i]);
					return true;
				}
		}
		// при поиске дома датчик дома может быть и хард лимитом, тогда резко не тормозим
		else
		{
			for (int i = 0; i < MAX_AXES; ++i)
				if (size[i] != 0) //TODO для activeSwitch вроде бы неправильный индекс
					if (activeSwitch[i] != -1 && // если концевик задан, и не равен концевику дома
						homeSwitch[i] != activeSwitch[i] && get_pin(activeSwitch[i])) // и сработал
						return true;
		}
		return false;
	}

	//---------------------------------------------------------------------
	// наезд на дом произошёл, оси остановлены
	bool home_reached()
	{
		return homeReached;
	}
	
	//---------------------------------------------------------------------
	// сброс перед вычислением нового значения
	void reset_home(bool value)
	{
		homeReached = value;
	}

	//---------------------------------------------------------------------
	// остановка при наезде на дом
	bool home_braking(float& axeVel, int i, int time, float velocity, float projection, float acceleration)
	{
		// если не задали концевик, но пытаемся на него наехать, то считаем что уже на нем
		if (homeSwitch[i] == -1 || get_pin(homeSwitch[i]) || homeActivated[i])
		{
			if (!homeActivated[i])
			{
				axeVelocity[i] = velocity / projection;
				timeActivate[i] = time;
				homeActivated[i] = true;
			}

			if (time - timeActivate[i] > (1<<30)) // страховка на случай переполнения таймера
				timeActivate[i] = time - (1<<30);

			axeVel = axeVelocity[i] - acceleration * (time - timeActivate[i]);
			if (axeVel >= 0)
				homeReached = false;
			return true;
		}
		else {
			homeReached = false;
		}

		return false;
	}

	//---------------------------------------------------------------------
	// задаём используемые для концевиков пины и полярность сигнала
	void process_packet_set_switches(PacketSetSwitches *packet)
	{
		char *pins = 0;
		switch (packet->group)
		{
			case SwitchGroup_MIN: pins = limitSwitchMin; break;
			case SwitchGroup_MAX: pins = limitSwitchMax; break;
			case SwitchGroup_HOME: pins = homeSwitch; break;
		}
		for (int i = 0; i < MAX_AXES; ++i)
			pins[i] = packet->pins[i];
		polarity = packet->polarity;
	}
};
