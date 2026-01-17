#pragma once
// исполняет команды

#include "receiver.h"
#include "track.h"
#include "packets.h"
#include "motor.h"
#include "sys_timer.h"
#include "led.h"
#include "math.h"
#include "spindle.h"
#include "feed.h"
#include "inertial.h"


void send_packet_service_command(PacketCount number);


//=====================================================================================================
int isign(int value)
{
	if(value > 0) return 1;
	else if (value < 0) return -1;
	else return 0;
}


//=========================================================================================
class Mover
{
public:
	float maxVelocity[MAX_AXES];      // мм/мкс
	float maxAcceleration[MAX_AXES];  // мм/мкс^2
	float stepLength[MAX_AXES]; // мм/шаг

	int coord[MAX_AXES];    //текущие координаты
	int stopTime;             //время следующей остановки

	int from[MAX_AXES];     //откуда двигаемся
	int to[MAX_AXES];       //куда двигаемся

	MoveMode interpolation;
	bool needPause;           //сбросить скорость до 0
	char breakState;          //сбросить очередь команд
	FeedModifier feedModifier;//управление подачей
	Inertial inertial;        //данные для корректного изменения скорости

	//данные, отвечающие за раздельное движение по осям (концевики и ручное управление)
	char limitSwitchMin[MAX_AXES]; //концевики
	char limitSwitchMax[MAX_AXES];
	char activeSwitch[MAX_AXES]; //концевики, в сторону которых сейчас едем
	int  activeSwitchCount;      //количество активных концевиков

	char homeSwitch[MAX_AXES]; //датчики дома
	float axeVelocity[MAX_AXES]; //скорость оси во время попадания в концевик
	int timeActivate[MAX_AXES]; //время начала остановки
	bool homeActivated[MAX_AXES]; //при наезде на дом выставляем бит
	bool homeReached;

	//медленный ШИМ
	int pwmSlowPeriod;
	int lastPwmUpdate;
	int pwmSizes[MAX_SLOW_PWMS];

bool canLog;

	//расчёт ускорения не совсем корректен
	//при движении по кривой максимальное ускорение меняется из-за изменения проекции на оси
	//и расстояние торможения неизвестно
	//но на малых скоростях это не должно проявляться
	//поскольку расстояние торможения маленькое
	//и касательная к кривой поворачивается не сильно

	//=====================================================================================================
	enum OperateResult
	{
		END = 1,
		WAIT,
	};


	//=====================================================================================================
	/*
	диапазоны
	скорость:
	1 шаг/40 сек
	1 000 000 шаг/сек   (если 0.0001 мм/шаг)

	для float
	сложение 147 тактов
	вычитание 153
	умножение 209
	деление 597 (на M0 ядре)
	сравнение 137
	*/

	struct LinearData
	{
		bool enabled;          //моторы включены
		int refCoord;          //индекс координаты, по которой шагаем
		int size[MAX_AXES];  //размеры линии
		int err[MAX_AXES];   //ошибка координат (внутреннее представление)
		int error[MAX_AXES]; //ошибка координат
		int sign[MAX_AXES];  //изменение координаты при превышении ошибки {-1 || 1}
		int last[MAX_AXES+1];   //последние координаты, для которых была посчитана ошибка
		float velCoef[MAX_AXES]; //на что умножить скорость, чтобы получить число тактов на шаг, мм/шаг

		float invProj;			//(длина опорной координаты / полную длину)
	};
	LinearData linearData;

	
	//=====================================================================================================
	//наезд на хард лимит
	bool switch_reached()
	{
		//при обычной езде при налете на концевик прекращаем выдавать STEP
		if (interpolation != MoveMode_HOME)
		{
			for (int i = 0; i < activeSwitchCount; ++i)
				if (activeSwitch[i] != -1 && get_pin(activeSwitch[i]))
				{
					log_console("switch %d, %d reached\n", i, activeSwitch[i]);
					return true;
				}
		}
		//при поиске дома датчик дома может быть и хард лимитом
		else
		{
			for (int i = 0; i < MAX_AXES; ++i)
				if (linearData.size[i] != 0)
					if (activeSwitch[i] != -1 && //если концевик задан, и не равен концевику дома
						homeSwitch[i] != activeSwitch[i] && get_pin(activeSwitch[i])) //и сработал
						return true;
		}
		return false;
	}

	//=====================================================================================================
	//наезд на дом
	bool home_reached()
	{
		return (interpolation == MoveMode_HOME) && homeReached;
	}
	
	//=====================================================================================================
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

	//=====================================================================================================
	void stop_motors()
	{
		if (linearData.enabled)
		{
			for (int i = 0; i < MAX_AXES; ++i)
				motor[i].stop();
			linearData.enabled = false;
		}
	}

	//=====================================================================================================
	void compute_error()
	{
		int ref = linearData.refCoord;
		//допустим, размеры по x, y = a, b  , отрезок начинается с 0
		//тогда уравнение будет a*y = b*x;   a*dy = b*dx;  a*dy - b*dx = 0
		//ошибка по y относительно x при смещении:  (a*dy - b*dx) / a
		int dx = -(virtualAxe._position - linearData.last[MAX_AXES]); //находим изменение опорной координаты
		linearData.last[MAX_AXES] = virtualAxe._position;
		for (int i = 0; i < MAX_AXES; ++i)
		{
			int dy = (motor[i]._position - linearData.last[i]) * linearData.sign[i]; //находим изменение текущей координаты
			int derr = dy * linearData.size[ref] - dx * linearData.size[i]; //находим ошибку текущей координаты//какой должен быть знак?
			linearData.err[i] += derr;
			linearData.last[i] = motor[i]._position;
		}
	}

	//=====================================================================================================
	void floor_error()
	{
		int ref = linearData.refCoord;
		for (int i = 0; i < MAX_AXES; ++i)
		{
			int floorAdd = linearData.size[ref] / 2 * isign(linearData.err[i]);
			linearData.error[i] = (linearData.err[i] + floorAdd) / linearData.size[ref];
		}
	}
	
	//=====================================================================================================
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
		//задаем скорости
		if (inertial.velocity == 0)
		{
			for (int i = 0; i < MAX_AXES; ++i)
				motor[i].set_period(MAX_STEP_TIME);
			virtualAxe.set_period(MAX_STEP_TIME);
		}
		else
		{
			int ref = linearData.refCoord;
			int curTime = timer.get_ticks();
			int stepTimeArr[MAX_AXES+1];
			float errCoef = 1.0f / linearData.size[ref];
			homeReached = true;
			for (int i = 0; i < MAX_AXES; ++i)
			{
				if (linearData.velCoef[i] == 0)
				{
					stepTimeArr[i] = MAX_STEP_TIME;
					continue;
				}
				else if (interpolation == MoveMode_HOME)
				{
					//если не задали концевик, но пытаемся на него наехать, то считаем что уже на нем
					if (homeSwitch[i] == -1 || get_pin(homeSwitch[i]) || homeActivated[i])
					{
						if (!homeActivated[i])
						{
							axeVelocity[i] = inertial.velocity / linearData.velCoef[i];
							timeActivate[i] = curTime;
							homeActivated[i] = true;
						}
						
						if (curTime - timeActivate[i] > (1<<30)) //страховка на случай переполнения таймера
							timeActivate[i] = curTime - (1<<30);
							
						float vel = axeVelocity[i] - maxAcceleration[i] * (curTime - timeActivate[i]);
						int stepTime = 1 / vel;
						if (stepTime > MAX_STEP_TIME || stepTime <= 0)
							stepTime = MAX_STEP_TIME;
						stepTimeArr[i] = stepTime;
						if (vel >= 0)
							homeReached = false;
						continue;
					}
					else {
						homeReached = false;
					}
				}

				float step = linearData.velCoef[i] / inertial.velocity;
				int err = linearData.err[i];
				float add = step * step * err * errCoef / 1024; //t*(1+t*e/T)
				float maxAdd = step * 0.1f; //регулировка скорости максимально на 10 %
				if (fabsf(add) > maxAdd)
					add = copysign(maxAdd, add);
				int stepTime = step + add;
				
				if (stepTime > MAX_STEP_TIME || stepTime <= 0) //0 возможен при переполнении разрядов
					stepTime = MAX_STEP_TIME;
				stepTimeArr[i] = stepTime;
			}
			//для виртуальной оси тоже считаем скорость
			{
				int stepTime = linearData.velCoef[ref] / inertial.velocity;
				if (stepTime > MAX_STEP_TIME || stepTime <= 0) //0 возможен при переполнении разрядов
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

	//=====================================================================================================
	bool brez_step()
	{
		int time = timer.get_ticks();
		for (int i = 0; i < MAX_AXES; ++i) //быстро запоминаем текущее состояние координат
			if (linearData.size[i] != 0 && motor[i]._isHardware)
				motor[i].shot(time);
		virtualAxe.shot(time); //виртуальная ось самая большая, если она = 0, то выйдет еще при инициализации
			
		//находим ошибку новых координат
		compute_error();
		
		int ref = linearData.refCoord;
		
		for (int i = 0; i < MAX_AXES; ++i) //медленными осями шагаем точно
			if (linearData.size[i] != 0 && !motor[i]._isHardware)
				if (motor[i].can_step(time) && linearData.err[i] < -linearData.size[ref] / 2)
				{
					motor[i].one_step(time);
					linearData.err[i] += linearData.size[ref];
					linearData.last[i] += linearData.sign[i];
				}
		
		floor_error();
		
		for (int i = 0; i < MAX_AXES; ++i)
			coord[i] = motor[i]._position;

		//обновляем скорости двигателей
		set_velocity();
		
		if(virtualAxe._position <= 0) //если дошли до конца, выходим
			return false;
			
		return true;
	}


	//=====================================================================================================
	bool brez_init(int dest[MAX_AXES])
	{
		bool diff = false;
		for(int i = 0; i < MAX_AXES; ++i) //для алгоритма рисования
		{
			from[i] = to[i];       //куда должны были доехать
			if (interpolation != MoveMode_HOME)
				to[i] = dest[i];       //куда двигаемся
			else
				to[i] += dest[i]; //инкрементальный режим для хоминга, так как текущие координаты на компьютере могут быть сбиты
			if(to[i] > from[i])
			{
				linearData.size[i] = to[i] - from[i]; //увеличение ошибки
				linearData.sign[i] = 1;                 //изменение координаты
			}
			else
			{
				linearData.size[i] = from[i] - to[i];
				linearData.sign[i] = -1;
			}

			if(to[i] != from[i])
				diff = true;
		}

		int ref = linearData.refCoord;
		virtualAxe._position = (to[ref] - coord[ref]) * linearData.sign[ref];
		//инициализируем ошибку, учитывая, что в начале отрезка она = 0
		for(int i = 0; i < MAX_AXES; ++i)
		{
			linearData.err[i] = 0;
			linearData.last[i] = from[i];
		}
		linearData.last[MAX_AXES] = linearData.size[ref];
		compute_error();

		return diff;
	}
	
	//=====================================================================================================
	void finish_linear()
	{
		stop_motors();
		for(int i = 0; i < MAX_AXES; ++i)
			to[i] = coord[i];
		inertial.state = 0;
		init_empty();
	}

	//=====================================================================================================
	OperateResult linear()
	{
		if (switch_reached())
		{
			finish_linear(); //TODO сделать обработку остановки
			return END;
		}
		
		if (home_reached())
		{
			finish_linear();
			return END;
		}
			
		float length = linearData.invProj * virtualAxe._position;
		length += 0.001f * tracks.current_length();

		float targetFeed = inertial.maxFeedVelocity;
		if (interpolation == MoveMode_LINEAR)
			feedModifier.modify(targetFeed, inertial.velocity, inertial.acceleration, coord, stepLength, linearData.velCoef);

		inertial.update_velocity(needPause ? 0 : targetFeed, length);

		if (!brez_step())
		{
			if (tracks.current_length() == 0)
			{
				stop_motors();
				inertial.state = 0;
			}
			return END;
		}
		
		//прерывание обработки
		if (inertial.velocity == 0 && breakState == 1)
		{
			finish_linear();
			receiver.init();
			tracks.init();
			breakState = 2;
			return END;
		}

		return WAIT;
	}


	//=====================================================================================================
	void init_linear(int dest[MAX_AXES], int refCoord, float acceleration, int uLength, float length, float velocity)
	{
		if (tracks.decrement(uLength)) {
			inertial.velocity = 0;
			inertial.state = 0;
		}

		linearData.refCoord = refCoord; //используется в brez_init

		if(!brez_init(dest)) //если двигаться никуда не надо, то выйдет на первом такте
		{
			log_console("ERR: brez %d, %d, %d\n", dest[0], dest[1], dest[2]);
			return;
		}

		start_motors();

		inertial.acceleration = acceleration;
		inertial.maxFeedVelocity = velocity;
		activeSwitchCount = 0;

		for (int i = 0; i < MAX_AXES; ++i)
		{
			motor[i].set_direction(linearData.sign[i] > 0);
			
			if (linearData.size[i] == 0)
				linearData.velCoef[i] = 0;
			else
			{
				linearData.velCoef[i] = length / linearData.size[i];
				
				int lim = linearData.sign[i] > 0 ? limitSwitchMax[i] : limitSwitchMin[i];
				if (interpolation == MoveMode_HOME)
						activeSwitch[i] = lim; //при движении домой торможение раздельное
				else
					if (lim != -1)
						activeSwitch[activeSwitchCount++] = lim; //иначе нас интересует любое срабатывание
				homeActivated[i] = false;
			}
		}
		
		linearData.invProj = linearData.velCoef[refCoord];
		homeReached = false;
		
		handler = &Mover::linear;
	}


	//=====================================================================================================
	OperateResult empty()
	{
		if((unsigned int)timer.get() % 1200000 > 600000)
			led.show(0);
		else
			led.hide(0);

		if (breakState == 1)
		{
			receiver.init();
			tracks.init();
			breakState = 2;
		}
	
		if(receiver.queue_empty())
			return WAIT;
		else
			return END;
	}


	//=====================================================================================================
	void init_empty()
	{
		handler = &Mover::empty;
	}


	//=====================================================================================================
	OperateResult wait()
	{
		if (breakState == 1)
		{
			receiver.init();
			tracks.init();
			breakState = 2;
			return END;
		}
		
		if(timer.check(stopTime))
			return WAIT;
		else
			return END;
	}


	//=====================================================================================================
	void init_wait(int delay)
	{
		stopTime = timer.get_mks(delay);
		handler = &Mover::wait;
	}

	//=====================================================================================================
	void updateSlowPwms(int time)
	{
		int delta = time - lastPwmUpdate;
		if (delta >= pwmSlowPeriod)
		{
			lastPwmUpdate = time;
			delta = 0;
		}
		for (int i = 0; i < MAX_SLOW_PWMS; ++i)
			set_pwm_pin(i, delta < pwmSizes[i]);
	}


	//=====================================================================================================
	void process_packet_move(PacketMove *packet)
	{
		switch (interpolation)
		{
		case MoveMode_LINEAR:
		case MoveMode_HOME:
			{
				led.flip(0);
				int coord[MAX_AXES];
				for (int i = 0; i < MAX_AXES; ++i)
					coord[i] = packet->coord[i];
				init_linear(coord, packet->refCoord, packet->acceleration, packet->uLength, packet->length, packet->velocity);
				break;
			}
		}
	}


	//=====================================================================================================
	typedef OperateResult (Mover::*Handler)();
	Handler handler;

	void update()
	{
		OperateResult result = (this->*handler)();
		if(result == END) //если у нас что-то шло и кончилось
		{
			if(receiver.queue_empty())
			{
				init_empty();
				empty();
			}
			else
			{
				const PacketCommon* common = (PacketCommon*)&receiver.queue.Front();
				send_packet_service_command(common->packetNumber);
				
				switch(common->command)
				{
				case DeviceCommand_MOVE:
					{
						process_packet_move((PacketMove*)common);
						break;
					}
				case DeviceCommand_MOVE_MODE:
					{
						interpolation = ((PacketInterpolationMode*)common)->mode;

						break;
					}
				case DeviceCommand_SET_VEL_ACC:
				{
					PacketSetVelAcc *packet = (PacketSetVelAcc*)common;
					float mks = 0.000001;
					for(int i = 0; i < MAX_AXES; ++i)
					{
						maxVelocity[i] = packet->maxVelocity[i] * mks;
						maxAcceleration[i] = packet->maxAcceleration[i] * mks * mks;
						log_console("[%d]: maxVel %d, maxAcc %d\n", i, int(packet->maxVelocity[i]), int(packet->maxAcceleration[i]));
					}
					break;
				}
				case DeviceCommand_SET_SWITCHES:
				{
					PacketSetSwitches *packet = (PacketSetSwitches*)common;
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
					break;
				}
				case DeviceCommand_SET_SPINDLE_PARAMS:
				{
					PacketSetSpindleParams *packet = (PacketSetSpindleParams*)common;
					spindle.pinNumber = packet->pin;
					spindle.marksCount = packet->marksCount;
					spindle.maxSyncPeriod = TIMER_FREQUENCY / packet->frequency; // период оборота в микросекундах
					spindle.sensorFilterSize = packet->filterSize;
					break;
				}
				case DeviceCommand_SET_COORDS:
				{
					PacketSetCoords *packet = (PacketSetCoords*)common;
					for (int i = 0; i < MAX_AXES; ++i)
						if (packet->used | (1 << i))
						{
							coord[i] = packet->coord[i];
							motor[i]._position = coord[i];
							to[i] = coord[i];
						}
					break;
				}
				case DeviceCommand_SET_FEED:
					{ //TODO возможно надо удалить пакет
						/*PacketSetFeed *packet = (PacketSetFeed*)common;
						linearData.feedVelocity = packet->feedVel;
						log_console("feed %d\n", int(linearData.feedVelocity));*/
						break;
					}
				case DeviceCommand_SET_FEED_MODE:
				{
					feedModifier.set_mode((PacketSetFeedMode*)common);
					break;
				}
				case DeviceCommand_SET_STEP_SIZE:
					{
						PacketSetStepSize *packet = (PacketSetStepSize*)common;
						for(int i = 0; i < MAX_AXES; ++i)
						{
							stepLength[i] = packet->stepSize[i];
							float mk = stepLength[i]*1000;
							int z = int(mk);
							log_console("[%d]: stepLength %d.%02d um\n", i, z, int(mk*100 - z*100));
						}
						break;
					}
				case DeviceCommand_SET_PWM:
					{
						PacketSetPWM *packet = (PacketSetPWM*)common;
						if (packet->pin < MAX_SLOW_PWMS + MAX_PWM)
						{
							if (packet->pin < MAX_SLOW_PWMS)
								pwmSizes[(int)packet->pin] = pwmSlowPeriod * packet->value;
							else
								set_pwm_width(MAX_SLOW_PWMS - packet->pin, TIM1->ARR * packet->value);
						}
						break;
					}
				case DeviceCommand_SET_PWM_FREQ:
					{
						PacketSetPWMFreq *packet = (PacketSetPWMFreq*)common;
						pwmSlowPeriod = 1000000 / packet->slowFreq;
						set_pwm_period(CORE_FREQ / packet->freq);
						break;
					}
				case DeviceCommand_WAIT:
					{
						PacketWait *packet = (PacketWait*)common;
						init_wait(packet->delay);
						break;
					}
				case DeviceCommand_SET_FRACT:
					break;

				default:
					log_console("ERR: undefined packet type %d, %d\n", common->command, common->packetNumber);
					break;
				}

				receiver.queue.Pop();
			}
		}

		int time = timer.get();
		updateSlowPwms(time);
		spindle.update(time);
		feedModifier.update(time);
	}


	//=====================================================================================================
	void init()
	{
		for(int i = 0; i < MAX_AXES; i++)
		{
			coord[i] = 0;
			limitSwitchMin[i] = -1;
			limitSwitchMax[i] = -1;
			homeSwitch[i] = -1;
		}

		needPause = false;
		breakState = 0;
		stopTime = 0;
		handler = &Mover::empty;

		interpolation = MoveMode_LINEAR;
		feedModifier = FeedModifier();
		spindle = Spindle();

		pwmSlowPeriod = PWM_SLOW_SIZE;
		for(int i = 0; i < MAX_SLOW_PWMS; i++)
			pwmSizes[i] = 0;
		lastPwmUpdate = 0;
	}
};

Mover mover;


//=====================================================================================================
// для лукахеда как только добавляется непрерывный массив паков в очереди
// сразу считаем доступное до остановки расстояние
void preprocess_command(PacketCommon *p)
{
	switch(p->command)
	{
	case DeviceCommand_SET_FRACT:
		{
			tracks.new_track();
			break;
		}
	case DeviceCommand_MOVE:
		{
			tracks.increment(((PacketMove*)p)->uLength);
			break;
		}
	}
}


//=========================================================================================
// обработка специальных пакетов, которые не попадают в основную очередь
bool execute_nonmain_packet(PacketCommon* common)
{
	switch (common->command)
	{
		case DeviceCommand_RESET_PACKET_NUMBER:
		{
			receiver.reinit();
			mover.breakState = 0;
			send_packet_received(-1, 0);
			return true;
		}
		case DeviceCommand_SET_FEED_MULT:
		{
			if (receiver.packet_received2(common))
			{
				PacketSetFeedMult *packet = (PacketSetFeedMult*)common;
				mover.feedModifier.feedMult = packet->feedMult;
				log_console("feedMult %d%%\n", int(mover.feedModifier.feedMult*100));
			}
			return true;
		}
		case DeviceCommand_PAUSE:
		{
			if (receiver.packet_received2(common))
			{
				PacketPause *packet = (PacketPause*)common;
				mover.needPause = (packet->needPause != 0);
				log_console("pause %d\n", int(mover.needPause));
			}
			return true;
		}
		case DeviceCommand_BREAK:
		{
			if (mover.breakState == 2)
			{
				send_packet_received(common->packetNumber, 1);
				mover.breakState = 0;
				mover.needPause = 0;
			}
			else if (mover.breakState == 0)
			{
				mover.breakState = 1;
				receiver.reinit();
				mover.needPause = 1;
				log_console("break %d\n", common->packetNumber);
			}
			return true;
		}
	}

	return false;
}
