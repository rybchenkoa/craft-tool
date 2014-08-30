#include "fifo.h"
#include "packets.h"
#include "motor.h"
#include "sys_timer.h"
#include "led.h"
#include "float16.h"

void send_packet(char *packet, int size);

class Receiver
{
	public:
	FIFOBuffer<PacketUnion, 5> queue; //28*32+8 = 904
	PacketCount packetNumber;
	
	void init()
	{
		queue.Clear();
		packetNumber = char(-1);
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
void send_packet_error_number(int number)
{
	PacketErrorPacketNumber packet;
	packet.command = DeviceCommand_ERROR_PACKET_NUMBER;
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
	//led.show();
	int crc = calc_crc(packet, size-4);
	int receivedCrc = *(int*)(packet+size-4);
	if(crc != receivedCrc)
	{
  	//log_console("\n0x%X, 0x%X\n", receivedCrc, crc);
		send_wrong_crc();
		return;
	}
	
	if(*packet == DeviceCommand_RESET_PACKET_NUMBER)
	{
		receiver.packetNumber = PacketCount(-1);
		send_packet_received(-1);
	}
	else
	{
		PacketCommon* common = (PacketCommon*)packet;
		if (common->packetNumber == receiver.packetNumber) //до хоста не дошёл ответ о принятом пакете
		{
			send_packet_received(receiver.packetNumber);                       //шлём ещё раз
			
			//log_console("\nBYLO %d %d\n", common->packetNumber, receiver.packetNumber);
		}
		else if(common->packetNumber == PacketCount(receiver.packetNumber + 1)) //принят следующий пакет
		{
			if(!receiver.queue.IsFull())
			{
				++(receiver.packetNumber);
				push_received_packet(packet, size); //при использовании указателей можно было бы не копировать ещё раз
				send_packet_received(receiver.packetNumber); //говорим, что приняли пакет
  			//log_console("\nGOTOV %d %d\n", common->packetNumber, receiver.packetNumber);
			}
		}
		else
		{
			send_packet_error_number(receiver.packetNumber);
			//log_console("\n%d %d\n", common->packetNumber, receiver.packetNumber);
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
	float16 stepLength[NUM_COORDS];      //мм/шаг
	
	int coord[NUM_COORDS];    //текущие координаты
	int bufCoord[NUM_COORDS]; //предрассчитанные новые координаты
	int velocity[NUM_COORDS]; //текущая скорость
	bool needStop;   //принудительная остановка
	int stopTime;    //время следующей остановки

	int nextBound[NUM_COORDS];//координаты, на которых надо затормозить
	
	int from[NUM_COORDS];     //откуда двигаемся
	int to[NUM_COORDS];       //куда двигаемся
	
	MoveMode interpolation;
	int rFeed;    //подача (тиков/мм)
	
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
		{
			if(length == 0) length = 1;
			linearData.rVelocity = isqrt(linearData.maxrAcceleration / (2*length));
			//log_console("len %d, acc %d\n", length, linearData.rVelocity);
		}
		else
			linearData.rVelocity = linearData.maxrVelocity;
		
		stopTime = timer.get_mks(currentTime, linearData.rVelocity);
		
		return WAIT;
	}
	
	//----------------------------------
	void init_linear(int dest[3], bool isMax)
	{
		bool isEqual = true;
		float16 lengths[NUM_COORDS];
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
			if(to[i] != from[i])
				isEqual = false;
				
			lengths[i] = linearData.delta[i];
		}
		
		if(isEqual) //если двигаться никуда не надо, то выйдет на первом такте
			return;
		
		linearData.refCoord = 0;
		linearData.refDelta = linearData.delta[0];     //находим опорную координату (максимальной длины)
		for(int i = 1; i < NUM_COORDS; ++i)
			if(linearData.refDelta < linearData.delta[i])
			{
				linearData.refDelta = linearData.delta[i];
				linearData.refCoord = i;
			}
		
		//для алгоритма ускорения
		int ref = linearData.refCoord;
		int refLen = linearData.delta[ref]; //проекция отрезка на опорную координату
		
		//находим максимальную скорость по опорной координате
		float16 timeMax = lengths[0] * float16(maxrVelocity[0]);
		for(int i = 1; i < NUM_COORDS; ++i)
		{
			float16 time = lengths[i] * float16(maxrVelocity[i]);
			if(timeMax < time)
				timeMax = time;
		}
		linearData.maxrVelocity = timeMax / float16(refLen);
		
		//ограничиваем скорость подачей
		if(!isMax)
		{
			float16 sqLen = pow2(lengths[0] * stepLength[0]); //мм
			for(int i = 1; i < NUM_COORDS; ++i)
				sqLen += pow2(lengths[i] * stepLength[i]);
		
			float16 length = sqrt(sqLen);    //длина отрезка в мм
			float16 projLen = lengths[ref] / length; //косинус скорости на опорной координате
			//тик/мм * мм/шаг 
			int projFeedVelocity = int(float16(rFeed) / projLen); //проекция скорости подачи на опорную координату
			if(linearData.maxrVelocity < projFeedVelocity) //скорость подачи < макс. достижимой
				linearData.maxrVelocity = projFeedVelocity;
		}
		
		//находим максимальное ускорение по опорной координате
		timeMax = lengths[0] * float16(maxrAcceleration[0]);
		for(int i = 1; i < NUM_COORDS; ++i)
		{
			float16 time = lengths[i] * float16(maxrAcceleration[i]);
			if(timeMax < time)
				timeMax = time;
		}
		linearData.maxrAcceleration = timeMax / lengths[ref];
		
		//теперь смотрим, достигнем ли такой скорости, когда дойдём до середины отрезка
		//v=a*t, s=a*t^2/2 =v^2/2a
		int accLength = float16(linearData.maxrAcceleration)/(float16(linearData.maxrVelocity)*float16(linearData.maxrVelocity)) ;
		accLength /= 2;
		//int accLength = linearData.maxrAcceleration/linearData.maxrVelocity;
		//accLength /= (2 * linearData.maxrVelocity);
		//log_console("acc %d, vel %d\n", linearData.maxrAcceleration, linearData.maxrVelocity);
		//log_console("len %d, len2 %d\n", accLength, refLen);
		if(accLength > refLen/2)
		{
			accLength = refLen/2;
			linearData.maxrVelocity = isqrt(linearData.maxrAcceleration / refLen);
		}
		linearData.accLength = accLength;
		//log_console("len %d, acc %d, vel %d, ref %d\n", accLength, linearData.maxrAcceleration, linearData.maxrVelocity, ref);
		handler = &Mover::linear;
	}

	//----------------------------------
	OperateResult empty()
	{
		if((unsigned int)timer.get() % 12000000 > 6000000)
			led.show();
		else
			led.hide();
			
		if(receiver.queue.IsEmpty())
		{
			stopTime = timer.get_ms(10);
			return WAIT;
		}
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
		motor[0].set_sin_voltage(bufCoord[0], 255);
		motor[1].set_sin_voltage(bufCoord[0], 255);
		motor[2].set_sin_voltage(bufCoord[1], 255);
		motor[3].set_sin_voltage(bufCoord[2], 255);
		
		for(int i = 0; i < NUM_COORDS; ++i)
			coord[i] = bufCoord[i];
		
		OperateResult result = (this->*handler)();
		if(result == END /* && this->handler != &Mover::empty*/) //если у нас что-то шло и кончилось
		{
			//led.hide();
			if(receiver.queue.IsEmpty())
			{
				init_empty();
				empty();
			}
			else
			{
			  //log_console("DO  first %d, last %d\n", receiver.queue.first, receiver.queue.last);	

				const PacketCommon* common = (PacketCommon*)&receiver.queue.Front();
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
								led.flip();

			//log_console("pos %7d, %7d, %5d, time %d init\n",
			        //packet->coord[0], packet->coord[1], packet->coord[2], timer.get());
								
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

			      //log_console("posle1  first %d, last %d\n", receiver.queue.first, receiver.queue.last);
						break;
					}
					case DeviceCommand_MOVE_MODE:
					{
						interpolation = ((PacketInterpolationMode*)common)->mode;
						receiver.queue.Pop();
						log_console("mode %d\n", interpolation);
            //log_console("posle2  first %d, last %d\n",
			      //     receiver.queue.first, receiver.queue.last);
						
						break;
					}
					case DeviceCommand_SET_BOUNDS:
					{
						PacketSetBounds *packet = (PacketSetBounds*)common;
						for(int i = 0; i < NUM_COORDS; ++i)
						{
							minCoord[i] = packet->minCoord[i];
							maxCoord[i] = packet->maxCoord[i];
						}
						receiver.queue.Pop();
						break;
					}
					case DeviceCommand_SET_VEL_ACC:
					{
						PacketSetVelAcc *packet = (PacketSetVelAcc*)common;
						for(int i = 0; i < NUM_COORDS; ++i)
						{
							maxrVelocity[i] = packet->maxrVelocity[i];
							maxrAcceleration[i] = packet->maxrAcceleration[i];
							log_console("[%d]: maxVel %d, maxAcc %d\n", i, maxrVelocity[i], maxrAcceleration[i]);
						}
						receiver.queue.Pop();
						break;
					}
					case DeviceCommand_SET_FEED:
					{
						PacketSetFeed *packet = (PacketSetFeed*)common;
						rFeed = packet->rFeed;
						log_console("rFeed %d\n", rFeed);
						receiver.queue.Pop();
						break;
					}
					case DeviceCommand_SET_STEP_SIZE:
					{
						PacketSetStepSize *packet = (PacketSetStepSize*)common;
						for(int i = 0; i < NUM_COORDS; ++i)
						{
							stepLength[i] = packet->stepSize[i];
							log_console("[%d]: stepLength %d\n", i, int(stepLength[i].exponent));
						}
						receiver.queue.Pop();
						break;
					}
					case DeviceCommand_SET_VOLTAGE:
					{
						PacketSetVoltage *packet = (PacketSetVoltage*)common;
						for(int i = 0; i < NUM_COORDS; ++i)
						{
							motor[i+1].maxVoltage = packet->voltage[i];
						}
						motor[0].maxVoltage = packet->voltage[0];
						receiver.queue.Pop();
						break;
					}
					case DeviceCommand_SET_PLANE:
					{
						receiver.queue.Pop();
						break;
					}
					case DeviceCommand_WAIT:
						receiver.queue.Pop();
						break;
						
					default:
						log_console("undefined packet type %d, %d\n", common->command, common->packetNumber);
						receiver.queue.Pop();
						break;
				}
				
			  //log_console("POSLE  first %d, last %d\n",
			  //      receiver.queue.first, receiver.queue.last);
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
		
		//это должно задаваться с компьютера
		for(int i = 0; i < NUM_COORDS; i++)
		{
			const float stepSize = 0.1f / SUB_STEPS; //0.1 мм на шаг
			const float mmsec = 100; //мм/сек
			const float delay = 0.000001; //1 тик - 1 микросекунда
			const float accel = 100;//мм/сек^2
			
			maxrVelocity[i] = 1/((mmsec/stepSize)*delay);
			maxrAcceleration[i] = 1/(accel/stepSize*delay*delay);
		}
		interpolation = MoveMode_FAST;
		rFeed = maxrVelocity[0] * 5; //для обычной подачи задержка больше
		
	}
};

Mover mover;
