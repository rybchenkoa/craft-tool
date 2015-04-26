#include "fifo.h"
#include "packets.h"
#include "motor.h"
#include "sys_timer.h"
#include "led.h"
#include "float16.h"

void send_packet(char *packet, int size);
void process_packet(char *packet, int size);

class Receiver
{
	public:
	FIFOBuffer<MaxPacket, 3> queue; //28*32+8 = 904
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
void push_received_packet(char * __restrict packet, int size)
{
	int size4 = (size+3)/4; //копируем сразу по 4 байта
	int *src = (int*)packet;
	int *dst = (int*)&receiver.queue.End();
	for(int i=0; i<size4; ++i)
		dst[i] = src[i];
	receiver.queue.Push();
}


//=========================================================================================
void send_packet_service_coords(int coords[NUM_COORDS])
{
	PacketServiceCoords packet;
	packet.command = DeviceCommand_SERVICE_COORDS;
	for(int i = 0; i < NUM_COORDS; ++i)
		packet.coords[i] = coords[i];
	packet.crc = calc_crc((char*)&packet, sizeof(packet) - 4);
	send_packet((char*)&packet, sizeof(packet));
}


//=========================================================================================
void send_packet_service_command(PacketCount number)
{
	PacketServiceCommand packet;
	packet.command = DeviceCommand_SERVICE_COMMAND;
	packet.packetNumber = number;
	packet.crc = calc_crc((char*)&packet, sizeof(packet) - 4);
	send_packet((char*)&packet, sizeof(packet));
}


//=========================================================================================
void on_packet_received(char * __restrict packet, int size)
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
	if(size > sizeof(MaxPacket))
		log_console("rec pack %d, max %d\n", size, sizeof(MaxPacket));
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
			process_packet(packet, size);
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


//=====================================================================================================
int iabs(int value)
{
	return value>0 ? value : -value;
}


//=====================================================================================================
int ipow2(int value)
{
	return value*value;
}


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
	int minCoord[NUM_COORDS]; //габариты станка
	int maxCoord[NUM_COORDS]; //должны определяться по концевым выключателям
	float16 maxVelocity[NUM_COORDS];      // мм/мкс
	float16 maxAcceleration[NUM_COORDS];  // мм/мкс^2
	float16 stepLength[NUM_COORDS]; // мм/шаг
	
	int coord[NUM_COORDS];    //текущие координаты
	int bufCoord[NUM_COORDS]; //предрассчитанные новые координаты
	int stopTime;             //время следующей остановки
	int startTime;            //время начала текущего тика обработки

	int from[NUM_COORDS];     //откуда двигаемся
	int to[NUM_COORDS];       //куда двигаемся
	
	MoveMode interpolation;
	bool needStop;            //принудительная остановка
	float16 feedMult;         //заданная из интерейса скорость движения
	int voltage[NUM_COORDS];  //регулировка напряжения на больших оборотах и при простое
	
	//расчёт ускорения не совсем корректен
	//при движении по кривой максимальное ускорение меняется из-за изменения проекции на оси
	//и расстояние торможения неизвестно
	//но на малых скоростях это не должно проявляться
	//поскольку расстояние торможения маленькое
	//и касательная к кривой поворачивается не сильно
	
//=====================================================================================================
	enum State
	{
		FAST = 0,
		LINEAR,
	};
	
	
//=====================================================================================================
	enum OperateResult
	{
		END = 1,
		WAIT,
	};
	
	
//=====================================================================================================
	struct LinearData
	{
		int refCoord;          //индекс координаты, по которой шагаем
		int refDelta;          //разность опорной координаты
		int delta[NUM_COORDS]; //увеличение ошибки округления координат
		int err[NUM_COORDS];   //ошибка округления координат
		int add[NUM_COORDS];   //изменение координаты при превышении ошибки {-1 || 1}
		
		float16 feedVelocity;   //скорость подачи
		float16 acceleration;  //ускорение, шагов/тик^2
		float16 velocity;      //скорость на прошлом шаге
		float16 accLength;     //расстояние, на котором можно набрать максимальную скорость
		float16 invProj;       //(длина опорной координаты / полную длину)
		int     lastPeriod;    //длительность предыдущего шага
		int state;
	};
	LinearData linearData;
	
	
//=====================================================================================================
	bool brez_step()
	{
		if(coord[linearData.refCoord] == to[linearData.refCoord]) //дошли до конца, выходим
			return false;

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
		return true;
	}
	
	
//=====================================================================================================
	bool brez_init(int dest[NUM_COORDS])
	{
		bool diff = false;
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
				diff = true;
		}
		
		return diff;	
	}
	
//=====================================================================================================
	OperateResult linear()
	{
		int reference = linearData.refCoord;
		if(!brez_step())
			return END;

		float16 length = linearData.invProj * float16(iabs(coord[reference] - to[reference]));
		// 1/linearData.velocity = тик/мм
		float16 currentFeed = linearData.feedVelocity * feedMult;

		if(needStop)
		{
			linearData.velocity -= linearData.acceleration * float16(linearData.lastPeriod);
			linearData.state = 0;
		}
		// v^2 = 2g*h; //length < linearData.accLength 
		else if(pow2(linearData.velocity) > (linearData.acceleration * length << 1))
		{
			linearData.velocity -= linearData.acceleration * float16(linearData.lastPeriod);
			linearData.state = 1;
		}
		else if(linearData.velocity < currentFeed)
		{
			linearData.velocity += linearData.acceleration * float16(linearData.lastPeriod);
			linearData.state = 2;
		}
		else
			linearData.state = 3;
			
		if(linearData.velocity.mantis <= 0)
		{
			linearData.velocity.mantis = 0;
			linearData.velocity.exponent = 0;
			linearData.lastPeriod = 10000;
		}
		else
			linearData.lastPeriod = linearData.invProj / linearData.velocity;

		stopTime = timer.get_mks(startTime, linearData.lastPeriod);

		return WAIT;
	}


//=====================================================================================================
	void init_linear(int dest[NUM_COORDS], int refCoord, float16 acceleration, float16 accLength, float16 invProj, float16 velocity)
	{
		if(!brez_init(dest)) //если двигаться никуда не надо, то выйдет на первом такте
			return;
		
		linearData.refCoord = refCoord;
		linearData.refDelta = linearData.delta[refCoord];     //находим опорную координату (максимальной длины)
		linearData.acceleration = acceleration;
		linearData.accLength = accLength;
		linearData.feedVelocity = velocity;
		linearData.invProj = invProj;
		linearData.velocity = 0;
		linearData.lastPeriod = 10000;
		//log_console("len %d, acc %d, vel %d, ref %d\n", accLength, linearData.maxrAcceleration, linearData.maxrVelocity, ref);
		
		for(int i=0; i<NUM_COORDS; ++i)
		  voltage[i] = 256;
			
		handler = &Mover::linear;
	}

	
//=====================================================================================================
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
	
	
//=====================================================================================================
	void init_empty()
	{
		handler = &Mover::empty;
		
		for(int i=0; i<NUM_COORDS; ++i)
		  voltage[i] = 64;
	}
	
	
//=====================================================================================================
	typedef OperateResult (Mover::*Handler)();
	Handler handler;
	
	void update()
	{
		startTime = timer.get();
		motor[0].set_sin_voltage(bufCoord[0], voltage[0]);
		motor[1].set_sin_voltage(bufCoord[0], voltage[0]);
		motor[2].set_sin_voltage(bufCoord[1], voltage[1]);
		motor[3].set_sin_voltage(bufCoord[2], voltage[2]);
		
		for(int i = 0; i < NUM_COORDS; ++i)
			coord[i] = bufCoord[i];
		
		OperateResult result = (this->*handler)();
		if(result == END) //если у нас что-то шло и кончилось
		{
			if(receiver.queue.IsEmpty())
			{
				init_empty();
				empty();
			}
			else
			{
			  //log_console("DO  first %d, last %d\n", receiver.queue.first, receiver.queue.last);	

				const PacketCommon* common = (PacketCommon*)&receiver.queue.Front();
				send_packet_service_command(common->packetNumber);
				//log_console("queue[%d] = %d\n", receiver.queue.first, common->command);
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
			 //       packet->coord[0], packet->coord[1], packet->coord[2], timer.get());
								send_packet_service_coords(coord);
								init_linear(packet->coord, packet->refCoord, packet->acceleration, packet->accLength, packet->invProj, packet->velocity);
								break;
							}
						}

			      //log_console("posle1  first %d, last %d\n", receiver.queue.first, receiver.queue.last);
						break;
					}
					case DeviceCommand_MOVE_MODE:
					{
						interpolation = ((PacketInterpolationMode*)common)->mode;
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
						break;
					}
					case DeviceCommand_SET_VEL_ACC:
					{
						PacketSetVelAcc *packet = (PacketSetVelAcc*)common;
						for(int i = 0; i < NUM_COORDS; ++i)
						{
							maxVelocity[i] = packet->maxVelocity[i];
							maxAcceleration[i] = packet->maxAcceleration[i];
							log_console("[%d]: maxVel %d, maxAcc %d\n", i, int(maxVelocity[i]), int(maxAcceleration[i]));
						}
						break;
					}
					case DeviceCommand_SET_FEED:
					{
						PacketSetFeed *packet = (PacketSetFeed*)common;
						linearData.feedVelocity = packet->feedVel;
						log_console("feed %d\n", int(linearData.feedVelocity));
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
						break;
					}
					case DeviceCommand_WAIT:
					{
						init_empty();
						PacketWait *packet = (PacketWait*)common;
						stopTime = timer.get_mks(packet->delay);
						break;
					}
						
					default:
						log_console("undefined packet type %d, %d\n", common->command, common->packetNumber);
						break;
				}
				
				receiver.queue.Pop();
				//log_console("POSLE  first %d, last %d\n",
				//      receiver.queue.first, receiver.queue.last);
			}
		}
	}
	
	
//=====================================================================================================
	void init()
	{
		for(int i = 0; i < NUM_COORDS; i++)
		{
			minCoord[i] = 0;
			maxCoord[i] = 0;
			coord[i] = 0;
			bufCoord[i] = 0;
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
			
			maxVelocity[i] = mmsec/stepSize*delay;
			maxAcceleration[i] = accel/stepSize*delay*delay;
		}
		interpolation = MoveMode_FAST;
		//feedVelocity = maxVelocity[0]; //для обычной подачи задержка больше
		feedMult = 1;

		for(int i=0; i<NUM_COORDS; ++i)
		  voltage[i] = 64;
	}
};

Mover mover;

void process_packet(char *common, int size)
{
	switch(((PacketCommon*)common)->command)
	{
		case DeviceCommand_SET_FEED_MULT:
		{
			PacketSetFeedMult *packet = (PacketSetFeedMult*)common;
			mover.feedMult = packet->feedMult;
			log_console("feed %d\n", int(mover.feedMult));
			break;
		}
		default:
		{
			if(!receiver.queue.IsFull())
			{
				++(receiver.packetNumber);
				push_received_packet(common, size); //при использовании указателей можно было бы не копировать ещё раз
				send_packet_received(receiver.packetNumber); //говорим, что приняли пакет
				//log_console("\nGOTOV %d %d\n", common->packetNumber, receiver.packetNumber);
			}
		}
	}
}
