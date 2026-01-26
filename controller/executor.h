#pragma once
// исполняет команды

#include <cmath>

#include "task_empty.h"
#include "task_wait.h"
#include "task_move.h"
#include "receiver.h"
#include "track.h"
#include "packets.h"
#include "sys_timer.h"
#include "led.h"
#include "spindle.h"
#include "pwm.h"


void send_packet_service_command(PacketCount number);


//=========================================================================================
class Executor
{
public:
	Task* handler;
	TaskEmpty taskEmpty; // ожидать команд
	TaskWait taskWait;   // ожидать заданное время
	TaskMove taskMove;   // линейное движение

	char breakState;     // сбросить очередь команд
	PwmController pwm;   // управление выходными пинами

bool canLog;


	//=====================================================================================================
	void init_empty()
	{
		handler = &taskEmpty;
	}


	//=====================================================================================================
	void init_wait(int delay)
	{
		taskWait.init(delay);
		handler = &taskWait;
	}


	//=====================================================================================================
	void init_move(PacketMove *packet)
	{
		led.flip(0);
		int coord[MAX_AXES];
		for (int i = 0; i < MAX_AXES; ++i)
			coord[i] = packet->coord[i];
		if (taskMove.init(coord, packet->refCoord, packet->acceleration,
				packet->uLength, packet->length, packet->velocity))
		{
			handler = &taskMove;
		}
	}


	//=====================================================================================================
	void execute_packet(const PacketCommon *common)
	{
		switch(common->command)
		{
			case DeviceCommand_MOVE:
			{
				init_move((PacketMove*)common);
				break;
			}
			case DeviceCommand_MOVE_MODE:
			{
				taskMove.interpolation = ((PacketInterpolationMode*)common)->mode;
				break;
			}
			case DeviceCommand_SET_VEL_ACC:
			{
				PacketSetVelAcc *packet = (PacketSetVelAcc*)common;
				taskMove.process_packet_set_vel_acc(packet);
				break;
			}
			case DeviceCommand_SET_SWITCHES:
			{
				PacketSetSwitches *packet = (PacketSetSwitches*)common;
				taskMove.switches.process_packet_set_switches(packet);
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
				taskMove.process_packet_set_coords(packet);
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
				taskMove.feedModifier.set_mode((PacketSetFeedMode*)common);
				break;
			}
			case DeviceCommand_SET_STEP_SIZE:
			{
				PacketSetStepSize *packet = (PacketSetStepSize*)common;
				taskMove.process_packet_set_step_size(packet);
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
	}


	//=====================================================================================================
	void update()
	{
		bool needBreak = (breakState == 1);
		Task::OperateResult result = handler->update(needBreak);
		if(result == Task::END) //если у нас что-то шло и кончилось
		{
			if (needBreak) {
				receiver.init();
				tracks.init();
				breakState = 2;
			}

			if(receiver.queue_empty())
			{
				init_empty();
				taskEmpty.update(false);
			}
			else
			{
				const PacketCommon* common = (PacketCommon*)&receiver.queue.Front();
				send_packet_service_command(common->packetNumber);
				execute_packet(common);
				receiver.queue.Pop();
			}
		}

		int time = timer.get();
		pwm.update(time);
		spindle.update(time);
	}


	//=====================================================================================================
	void init()
	{
		breakState = 0;
		handler = &taskEmpty;
		taskEmpty = TaskEmpty();
		taskWait = TaskWait();
		taskMove = TaskMove();
		spindle = Spindle();
		pwm = PwmController();
	}
};

Executor executor;


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
			executor.breakState = 0;
			send_packet_received(-1, 0);
			return true;
		}
		case DeviceCommand_SET_FEED_MULT:
		{
			if (receiver.packet_received2(common))
			{
				PacketSetFeedMult *packet = (PacketSetFeedMult*)common;
				executor.taskMove.feedModifier.feedMult = packet->feedMult;
				log_console("feedMult %d%%\n", int(executor.taskMove.feedModifier.feedMult*100));
			}
			return true;
		}
		case DeviceCommand_PAUSE:
		{
			if (receiver.packet_received2(common))
			{
				PacketPause *packet = (PacketPause*)common;
				executor.taskMove.needPause = (packet->needPause != 0);
				log_console("pause %d\n", int(executor.taskMove.needPause));
			}
			return true;
		}
		case DeviceCommand_BREAK:
		{
			// если уже остановились, сбрасываем флаги остановки
			if (executor.breakState == 2)
			{
				send_packet_received(common->packetNumber, 1);
				executor.breakState = 0;
				executor.taskMove.needPause = 0;
			}
			// для breakState == 1 не обрабатываем, потому что уже в процессе остановки
			else if (executor.breakState == 0)
			{
				executor.breakState = 1;
				receiver.reinit();
				executor.taskMove.needPause = 1;
				log_console("break %d\n", common->packetNumber);
			}
			return true;
		}
	}

	return false;
}
