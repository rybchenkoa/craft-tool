#pragma once
// обрабатывает приём пакетов

#include "fifo.h"
#include "packets.h"
#include "sys_timer.h"
#include "led.h"

// подключение к другим модулям
void send_packet(char *packet, int size);
void preprocess_command(PacketCommon *p);
bool execute_nonmain_packet(PacketCommon* common);

// сосбственные предварительные объявления
void send_wrong_crc();
void send_packet_error_number(int number);
void send_packet_queue_full(int number);
void send_packet_repeat(int number, int queue);
void send_packet_received(int number, int queue);


class Receiver
{
public:
	FIFOBuffer<MaxPacket, 5> queue; //48*32+8 = 904
	int _tail; //первый ожидаемый элемент очереди (он не заполнен, а дальше могут быть заполненные)
	PacketCount _index; //номер его пакета
	static const int BUFFER_LEN = 5;
	PacketCount _index2; //второе соединение, обработка на лету

	void init()
	{
		queue.Clear();
		_index = PacketCount(-1);
		_index2 = PacketCount(-1);
		_tail = queue.last;
	}
	
	bool queue_empty()
	{
		return queue.IsEmpty() || !queue.Front().fill;
	}
	
	void reinit()
	{
		//оставляем только непрерывный диапазон дошедших пакетов, отбросив все после первого не дошедшего
		queue.last = _tail;
		_index = PacketCount(0);
		_index2 = PacketCount(-1);
	}

	void save_received_packet(char * __restrict packet, int size, int pos)
	{
		int size4 = (size+3)/4; //копируем сразу по 4 байта
		int *src = (int*)packet;
		int *dst = (int*)&queue.Element(pos);
		for(int i=0; i<size4; ++i)
			dst[i] = src[i];
		((MaxPacket*)dst)->fill = 1;
	}

	void packet_received(PacketCommon *p, int size)
	{
		int offset = PacketCount(p->packetNumber - _index);
		if (offset < 0)
		{
			send_packet_repeat(p->packetNumber, 0);
			return;
		}
		if (offset > BUFFER_LEN)
		{
			send_packet_error_number(p->packetNumber);
			return;
		}
		int target = _tail + offset;
		while(queue.last <= target) //добавляем в очередь пустые пакеты перед этим пакетом
		{
			if (queue.IsFull())
			{
				send_packet_queue_full(p->packetNumber);
				return;
			}
			MaxPacket* element = &queue.End();
			element->fill = 0;
			queue.Push();
		}
		if (queue.Element(target).fill)
		{
			send_packet_repeat(p->packetNumber, 0);
			return;
		}
		save_received_packet((char*)p, size, target);
		send_packet_received(p->packetNumber, 0); //говорим, что приняли пакет
		
		//обрабатываем принятый сплошной список пакетов
		while(queue.Element(_tail).fill && (_tail < queue.last))
		{
			preprocess_command((PacketCommon*)&queue.Element(_tail));
			++_tail;
			++_index;
		}
	}

	bool packet_received2(PacketCommon *p)
	{
		if (p->packetNumber == PacketCount(_index2 - 1)) //до хоста не дошёл ответ о принятом пакете
		{
			send_packet_repeat(p->packetNumber, 1);         //шлём ещё раз
		}
		else if(p->packetNumber != _index2) //принят следующий пакет
		{
			send_packet_error_number(_index2);
		}
		else
		{
			send_packet_received(++_index2, 1);
			return true;
		}
		return false;
	}
};

Receiver receiver;


//=========================================================================================
void on_packet_received(char * __restrict packet, int size)
{
	led.flip(1);

	int crc = calc_crc(packet, size-4);
	int receivedCrc = *(int*)(packet+size-4);
	if(crc != receivedCrc)
	{
		send_wrong_crc();
		return;
	}

	if(size > (int)sizeof(MaxPacket))
	{
		send_wrong_crc();
		log_console("ERR: pack size %d, max %d\n", size, sizeof(MaxPacket));
		return;
	}

	PacketCommon* common = (PacketCommon*)packet;
	if (!execute_nonmain_packet(common)) {
		receiver.packet_received(common, size);
	}
}


//=========================================================================================
void send_wrong_crc()
{
	PacketErrorCrc packet;
	packet.command = DeviceCommand_PACKET_ERROR_CRC;
	packet.crc = calc_crc((char*)&packet, sizeof(packet) - 4);
	send_packet((char*)&packet, sizeof(packet));
}


//=========================================================================================
void send_packet_received(int number, int queue)
{
	PacketReceived packet;
	packet.command = DeviceCommand_PACKET_RECEIVED;
	packet.packetNumber = number;
	packet.queue = queue;
	packet.crc = calc_crc((char*)&packet, sizeof(packet) - 4);
	send_packet((char*)&packet, sizeof(packet));
}


//=========================================================================================
void send_packet_repeat(int number, int queue)
{
	PacketReceived packet;
	packet.command = DeviceCommand_PACKET_REPEAT;
	packet.packetNumber = number;
	packet.queue = queue;
	packet.crc = calc_crc((char*)&packet, sizeof(packet) - 4);
	send_packet((char*)&packet, sizeof(packet));
}


//=========================================================================================
void send_packet_queue_full(int number)
{
	PacketReceived packet;
	packet.command = DeviceCommand_QUEUE_FULL;
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
void send_packet_service_coords(int coords[MAX_AXES])
{
	PacketServiceCoords packet;
	packet.command = DeviceCommand_SERVICE_COORDS;
	for(int i = 0; i < MAX_AXES; ++i)
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
