#include "fifo.h"
#include "packets.h"
#include "motor.h"
#include "sys_timer.h"
#include <math.h>

void send_packet(char *packet, int size);

class Receiver
{
	public:
	FIFOBuffer<PacketUnion, 5> queue; //28*32+8 = 904
	PacketCount packetNumber;
	
	void init()
	{
		queue.Clear();
		packetNumber = 0;
	}
};

Receiver receiver;

//=========================================================================================
void send_wrong_crc()
{
	PacketErrorCrc packet;
	packet.command = DeviceCommand_PACKET_ERROR_CRC;
	packet.crc = calc_crc((char*)&packet, sizeof(packet) - 4);
	send_packet((char*)&packet, sizeof(packet));
}

//=========================================================================================
void send_packet_received(int number)
{
	PacketReceived packet;
	packet.command = DeviceCommand_PACKET_RECEIVED;
	packet.packetNumber = number;
	packet.crc = calc_crc((char*)&packet, sizeof(packet) - 4);
	send_packet((char*)&packet, sizeof(packet));
}

//=========================================================================================
void push_received_packet(char *packet, int size)
{
	int size4 = (size+3)/4; //копируем сразу по 4 байта
	int *src = (int*)packet;
	int *dst = (int*)&receiver.queue.End();
	for(int i=0; i<size4; ++i)
		dst[i] = src[i];
	receiver.queue.Push();
}

//=========================================================================================
void on_packet_received(char *packet, int size)
{
	int crc = calc_crc(packet, size);
	if(crc != 0xFFFFFFFF)
	{
		send_wrong_crc();
		return;
	}
	
	if(*packet == DeviceCommand_RESET_PACKET_NUMBER)
	{
		receiver.packetNumber = 0;
		send_packet_received(0);
	}
	else
	{
		PacketCommon* common = (PacketCommon*)packet;
		if (common->packetNumber == receiver.packetNumber) //до хоста не дошёл ответ о принятом пакете
			send_packet_received(receiver.packetNumber);                       //шлём ещё раз
		else if(common->packetNumber == receiver.packetNumber + 1) //принят следующий пакет
		{
			if(!receiver.queue.IsFull())
			{
				++receiver.packetNumber;
				push_received_packet(packet, size); //при использовании указателей можно было бы не копировать ещё раз
				send_packet_received(receiver.packetNumber); //говорим, что приняли пакет
			}
		}
	}
}
//=========================================================================================
//примерное вычисление корня
int isqrt(int value)
{
	if(value <= 0) return 0;

	int leftBits = __clz(value); //бит слева до первого значащего бита, 0 = сразу попался
	unsigned int minVal = 1 << ((31-leftBits)/2); //примерный корень
	/*unsigned int maxVal = minVal*2; //это его правая граница
	//методом половинного деления добиваем до точного значения, макс. 16 итераций
	while(minVal != maxVal)
	{
		int current = (minVal + maxVal)/2;
		int value2 = current * (current+1); //это не магия, ~= (a+0.5)*(a+0.5)
		if (value2 >= value)
			maxVal = current;
		else
			minVal = current+1;
	}
	return minVal;*/
	minVal = (value/minVal + minVal)/2;
	minVal = (value/minVal + minVal)/2;
	minVal = (value/minVal + minVal)/2;
	return minVal;	
}

int iabs(int value)
{
	return value>0 ? value : -value;
}

int ipow2(int value)
{
	return value*value;
}
//=========================================================================================
class Mover
{
public:
	int minCoord[NUM_COORDS]; //габариты станка
	int maxCoord[NUM_COORDS]; //должны определяться по концевым выключателям
	int maxrVelocity[NUM_COORDS];    // 1/максимальная скорость передвижения (мкс/шаг)
	int maxrAcceleration[NUM_COORDS];// 1/максимальное ускорение (мкс^2/шаг)
	//int stepLength[NUM_COORDS];  //длина одного шага
	
	int coord[NUM_COORDS];    //текущие координаты
	int bufCoord[NUM_COORDS]; //предрассчитанные новые координаты
	int velocity[NUM_COORDS]; //текущая скорость
	bool needStop;   //принудительная остановка
	int stopTime;    //время следующей остановки

	int nextBound[NUM_COORDS];//координаты, на которых надо затормозить
	
	int from[NUM_COORDS];     //откуда двигаемся
	int to[NUM_COORDS];       //куда двигаемся
	
	MoveMode interpolation;
	int rFeed;    //подача для G1
	int maxrFeed; //подача для G0
	
	//----------------------------------
	enum State
	{
		FAST = 0,
		LINEAR,
		CW,
		CCW,
		
	};
	
	//----------------------------------
	enum OperateResult
	{
		END = 1,
		WAIT,
	};
	
	//----------------------------------
	struct LinearData
	{
		int refCoord;          //индекс координаты, по которой шагаем
		int refDelta;          //разность опорной координаты
		int delta[NUM_COORDS]; //увеличение ошибки округления координат
		int err[NUM_COORDS];   //ошибка округления координат
		int add[NUM_COORDS];   //изменение координаты при превышении ошибки {-1 || 1}
		int maxrVelocity;      //максимальная скорость, тиков/шаг
		int maxrAcceleration;  //ускорение тиков^2/шаг
		int rVelocity;         //скорость на прошлом шаге
		int accLength;         //расстояние, в течение которого можно ускоряться
		//int unitVelocity[NUM_COORDS];  //единичная скорость
	};
	LinearData linearData;
	
	//----------------------------------
	OperateResult linear()
	{
		int currentTime = timer.get();
		int reference = linearData.refCoord;
		if(coord[reference] == to[reference]) //дошли до конца, выходим
			return END;
			
		for (int i = 0; i < NUM_COORDS; ++i)
		{
			linearData.err[i] += linearData.delta[i];     //считаем ошибку
			bufCoord[i] = coord[i];
			if(2 * linearData.err[i] >= linearData.refDelta)
			{
				bufCoord[i] += linearData.add[i];           //добавляем координату
				linearData.err[i] -= linearData.refDelta;   //вычитаем ошибку
			}
		}
		int length; // =1/v;  v=v0+a*t
		
		if((length = iabs(coord[reference] - to[reference])) < linearData.accLength ||
			 (length = iabs(coord[reference] - from[reference])) < linearData.accLength)
			linearData.rVelocity = isqrt(linearData.maxrAcceleration / (2*length));
		else
			linearData.rVelocity = linearData.maxrVelocity;
		
		stopTime = currentTime + linearData.rVelocity;
		
		return WAIT;
	}
	
	//----------------------------------
	void init_linear(int dest[3], bool isMax)
	{
		for(int i = 0; i < NUM_COORDS; ++i) //для алгоритма рисования
		{
			to[i] = dest[i];       //куда двигаемся
			from[i] = coord[i];    //откуда (текущие координаты)
			linearData.err[i] = 0; //накопленная ошибка
			if(to[i] > from[i])
			{
				linearData.delta[i] = to[i] - from[i]; //увеличение ошибки
				linearData.add[i] = 1;                 //изменение координаты
			}
			else
			{
				linearData.delta[i] = from[i] - to[i];
				linearData.add[i] = -1;
			}
		}
		
		linearData.refCoord = 0;
		linearData.refDelta = linearData.delta[0];     //находим опорную координату (максимальной длины)
		for(int i = 0; i < NUM_COORDS; ++i)
			if(linearData.refDelta < linearData.delta[i])
			{
				linearData.refDelta = linearData.delta[i];
				linearData.refCoord = i;
			}
		
		//для алгоритма ускорения
		int ref = linearData.refCoord;
		int refLen = iabs(to[ref] - from[ref]); //проекция отрезка на опорную координату
		
		//находим максимальную скорость по опорной координате
		int timeMax = iabs(to[0] - from[0]) * maxrVelocity[0];
		int sqLen = ipow2(to[0] - from[0]);
		for(int i = 0; i < NUM_COORDS; ++i)
		{
			int time = iabs(to[i] - from[i]) * maxrVelocity[i];
			if(timeMax < time)
				timeMax = time;
			sqLen += ipow2(to[i] - from[i]);
		}
		linearData.maxrVelocity = timeMax / refLen;
		
		int length = isqrt(sqLen);    //длина отрезка
		int projFeedVelocity = rFeed * length / refLen; //проекция скорости подачи на опорную координату
		if(linearData.maxrVelocity < projFeedVelocity) //скорость подачи < макс. достижимой
			linearData.maxrVelocity = projFeedVelocity;
		
		//находим максимальное ускорение по опорной координате
		timeMax = iabs(to[0] - from[0]) * maxrAcceleration[0];
		for(int i = 0; i < NUM_COORDS; ++i)
		{
			int time = iabs(to[i] - from[i]) * maxrAcceleration[i];
			if(timeMax < time)
				timeMax = time;
		}
		linearData.maxrAcceleration = timeMax / iabs(to[ref] - from[ref]);
		
		//теперь смотрим, достигнем ли такой скорости, когда дойдём до середины отрезка
		//v=a*t, s=a*t^2/2 =v^2/2a
		int accLength = linearData.maxrAcceleration/(2*linearData.maxrVelocity*linearData.maxrVelocity);
		if(accLength > refLen/2)
		{
			accLength = refLen/2;
			linearData.maxrVelocity = isqrt(linearData.maxrAcceleration / refLen);
		}
		linearData.accLength = accLength;
		
		handler = &Mover::linear;
	}

	//----------------------------------
	OperateResult empty()
	{
		if(receiver.queue.IsEmpty())
			return WAIT;
		else
			return END;
	}
	
	//----------------------------------
	void init_empty()
	{
		handler = &Mover::empty;
	}
	
	//----------------------------------
	typedef OperateResult (Mover::*Handler)();
	Handler handler;
	
	void update()
	{
		motor[0].set_sin_voltage(bufCoord[0], 64);
		motor[1].set_sin_voltage(bufCoord[0], 64);
		motor[2].set_sin_voltage(bufCoord[1], 64);
		motor[3].set_sin_voltage(bufCoord[2], 64);
		
		for(int i = 0; i < NUM_COORDS; ++i)
			coord[i] = bufCoord[i];
			
		if((this->*handler)() == END && this->handler != &Mover::empty) //если у нас что-то шло и кончилось
		{
			
			if(receiver.queue.IsEmpty())
			{
				init_empty();
				empty();
			}
			else
			{
				PacketCommon* common = (PacketCommon*)&receiver.queue.Front();
				switch(common->command)
				{
					case DeviceCommand_MOVE:
					{
						switch (interpolation)
						{
							case MoveMode_FAST:
							case MoveMode_LINEAR:
							{
								PacketMove *packet = (PacketMove*)common;
								init_linear(packet->coord, interpolation == MoveMode_FAST);
								break;
							}
							case MoveMode_CW_ARC:
							case MoveMode_CCW_ARC:
							{
								break;
							}
						}
						receiver.queue.Pop();
						break;
					}
					case DeviceCommand_MOVE_MODE:
					{
						interpolation = ((PacketInterpolationMode*)common)->mode;
						receiver.queue.Pop();
						break;
					}
					case DeviceCommand_SET_PLANE:
					{
					}
					case DeviceCommand_WAIT:
					case DeviceCommand_PACKET_ERROR_CRC:
					case DeviceCommand_PACKET_RECEIVED:
					case DeviceCommand_RESET_PACKET_NUMBER:
					break;
				}
			}
		}
	}
	
	//----------------------------------
	void init()
	{
		for(int i = 0; i < NUM_COORDS; i++)
		{
			minCoord[i] = 0;
			maxCoord[i] = 0;
			coord[i] = 0;
			bufCoord[i] = 0;
			velocity[i] = 0;
			nextBound[i] = 0;
		}
		
		needStop = false;
		stopTime = 0;
		handler = &Mover::empty;
	}
};

Mover mover;
