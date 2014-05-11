#include "fifo.h"
#include "packets.h"

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

class Mover
{
public:
	int minCoord[NUM_COORDS]; //габариты станка
	int maxCoord[NUM_COORDS]; //должны определяться по концевым выключателям
	int coord[NUM_COORDS];    //текущие координаты
	int bufCoord[NUM_COORDS]; //предрассчитанные новые координаты
	int velocity[NUM_COORDS]; //текущая скорость
	int nextBound[NUM_COORDS];//координаты, на которых надо затормозить
	bool needStop;   //принудительная остановка
	int stopTime;    //время следующей остановки
	
	int from[NUM_COORDS];     //откуда двигаемся
	int to[NUM_COORDS];       //куда двигаемся
	
	enum State
	{
		FAST = 0,
		LINEAR,
		CW,
		CCW,
		
	};
	
	enum OperateResult
	{
		END = 1,
		WAIT,
	};
	
	struct LinearData
	{
		int refCoord;          //индекс координаты, по которой шагаем
		int refDelta;          //разность опорной координаты
		int delta[NUM_COORDS]; //увеличение ошибки округления координат
		int err[NUM_COORDS];   //ошибка округления координат
		int add[NUM_COORDS];   //изменение координаты при превышении ошибки {-1 || 1}
	};
	LinearData linearData;
	
	int linear()
	{
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
		return WAIT;
	}
	
	void init_linear(int dest[3])
	{
		for(int i = 0; i < NUM_COORDS; ++i)
		{
			to[i] = dest[i];       //куда двигаемся
			from[i] = coord[i];    //откуда
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
		linearData.refDelta = linearData.delta[0];
		for(int i = 0; i < NUM_COORDS; ++i)
			if(linearData.refDelta < linearData.delta[i])
			{
				linearData.refDelta = linearData.delta[i];
				linearData.refCoord = i;
			}
	}

	//typedef int (*Handler)(Mover *t);
	typedef int (Mover::*Handler)();
	Handler handler;
	
	void update()
	{
		//void (*f)(Mover *t) = linear;
		//handler = linear;
		//int (Mover::*f)() = &Mover::linear;
		//(this->*f)();
		//handler(this);
		(this->*handler)();
	}
};

Mover mover;
