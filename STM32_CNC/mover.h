#include "fifo.h"
#include "packets.h"

class Usart;

class Mover
{
	public:
	FIFOBuffer<PacketUnion, 5> queue; //28*32+8 = 904
	PacketCount packetNumber;
};

Mover mover;

void send_packet(char *packet, int size);

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
	int *dst = (int*)&mover.queue.End();
	for(int i=0; i<size4; ++i)
		dst[i] = src[i];
	mover.queue.Push();
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
		mover.packetNumber = 0;
		send_packet_received(0);
	}
	else
	{
		PacketCommon* common = (PacketCommon*)packet;
		if (common->packetNumber == mover.packetNumber) //до хоста не дошёл ответ о принятом пакете
			send_packet_received(mover.packetNumber);                       //шлём ещё раз
		else if(common->packetNumber == mover.packetNumber + 1) //принят следующий пакет
		{
			++mover.packetNumber;
			push_received_packet(packet, size); //при использовании указателей можно было бы не копировать ещё раз
			send_packet_received(mover.packetNumber); //говорим, что приняли пакет
		}
	}
}
