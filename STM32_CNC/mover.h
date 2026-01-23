#pragma once
// исполняет команды

#include <cmath>

#include "receiver.h"
#include "track.h"
#include "packets.h"
#include "motor.h"
#include "discretization.h"
#include "sys_timer.h"
#include "led.h"
#include "spindle.h"
#include "feed.h"
#include "inertial.h"
#include "pwm.h"
#include "limit_switch.h"


void send_packet_service_command(PacketCount number);


//=========================================================================================
class Mover
{
public:
	float maxVelocity[MAX_AXES];      // мм/мкс
	float maxAcceleration[MAX_AXES];  // мм/мкс^2
	float stepLength[MAX_AXES]; // мм/шаг

	int coord[MAX_AXES];    //текущие координаты
	int stopTime;             //время следующей остановки

	MoveMode interpolation;
	bool needPause;           //сбросить скорость до 0
	char breakState;          //сбросить очередь команд
	FeedModifier feedModifier;//управление подачей
	Inertial inertial;        //данные для корректного изменения скорости
	LimitSwitchController switches; // хард лимиты и хоминг
	PwmController pwm;        // управление выходными пинами

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
		float error[MAX_AXES]; //ошибка координат
		float velCoef[MAX_AXES]; //на что умножить скорость, чтобы получить число тактов на шаг, мм/шаг
		float invProj;			//(длина опорной координаты / полную длину)
	};

	Discretization discretization; // вычисление позиции двигателей
	LinearData linearData;

	
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
					float add = step * step * err / 1024; //t*(1+t*e/T)
					float maxAdd = step * 0.1f; //регулировка скорости максимально на 10 %
					if (std::abs(add) > maxAdd)
						add = std::copysign(maxAdd, add);
					stepTime = step + add;
				}
				
				if (stepTime > MAX_STEP_TIME || stepTime <= 0) //0 возможен при переполнении разрядов
					stepTime = MAX_STEP_TIME;
				stepTimeArr[i] = stepTime;
			}
			//для виртуальной оси тоже считаем скорость
			{
				int stepTime = linearData.invProj / inertial.velocity;
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
	bool update_motors()
	{
		int time = timer.get_ticks();

		// обновляем позиции моторов
		discretization.update_position(time);

		// вычисляем ошибку позиций
		discretization.get_normalized_error(linearData.error);
		
		for (int i = 0; i < MAX_AXES; ++i)
			coord[i] = motor[i]._position;

		//обновляем скорости двигателей
		set_velocity();
		
		if(virtualAxe._position <= 0) //если дошли до конца, выходим
			return false;
			
		return true;
	}
	
	//=====================================================================================================
	void finish_linear()
	{
		stop_motors();
		discretization.finish(coord);
		inertial.stop();
		init_empty();
	}

	//=====================================================================================================
	OperateResult linear()
	{
		if (switches.switch_reached(interpolation == MoveMode_HOME, discretization.size))
		{
			finish_linear(); //TODO сделать обработку остановки
			return END;
		}
		
		if (switches.home_reached() && interpolation == MoveMode_HOME)
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

		if (!update_motors())
		{
			if (tracks.current_length() == 0)
			{
				stop_motors();
				inertial.stop();
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
			inertial.stop();
		}

		bool homing = (interpolation == MoveMode_HOME);

		//если двигаться никуда не надо, то выйдет на первом такте
		if(!discretization.init_segment(coord, dest, refCoord, homing))
		{
			log_console("ERR: brez %d, %d, %d\n", dest[0], dest[1], dest[2]);
			return;
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
	void process_packet_move(PacketMove *packet)
	{
		led.flip(0);
		int coord[MAX_AXES];
		for (int i = 0; i < MAX_AXES; ++i)
			coord[i] = packet->coord[i];
		init_linear(coord, packet->refCoord, packet->acceleration, packet->uLength, packet->length, packet->velocity);
	}


	//=====================================================================================================
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


	//=====================================================================================================
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


	//=====================================================================================================
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
					process_packet_set_vel_acc(packet);
					break;
				}
				case DeviceCommand_SET_SWITCHES:
				{
					PacketSetSwitches *packet = (PacketSetSwitches*)common;
					switches.process_packet_set_switches(packet);
					break;
				}
				case DeviceCommand_SET_SPINDLE_PARAMS:
				{
					PacketSetSpindleParams *packet = (PacketSetSpindleParams*)common;
					spindle.process_packet_set_spindle_params(packet);
					break;
				}
				case DeviceCommand_SET_COORDS:
				{
					PacketSetCoords *packet = (PacketSetCoords*)common;
					process_packet_set_coords(packet);
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
						process_packet_set_step_size(packet);
						break;
					}
				case DeviceCommand_SET_PWM:
					{
						PacketSetPWM *packet = (PacketSetPWM*)common;
						pwm.process_packet_set_pwm(packet);
						break;
					}
				case DeviceCommand_SET_PWM_FREQ:
					{
						PacketSetPWMFreq *packet = (PacketSetPWMFreq*)common;
						pwm.process_packet_set_pwm_freq(packet);
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
		pwm.update(time);
		spindle.update(time);
		feedModifier.update(time);
	}


	//=====================================================================================================
	void init()
	{
		for(int i = 0; i < MAX_AXES; i++) {
			coord[i] = 0;
		}

		needPause = false;
		breakState = 0;
		stopTime = 0;
		handler = &Mover::empty;

		interpolation = MoveMode_LINEAR;
		feedModifier = FeedModifier();
		spindle = Spindle();
		pwm = PwmController();
		switches = LimitSwitchController();
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
