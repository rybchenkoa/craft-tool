#include "fifo.h"
#include "packets.h"

class Mover
{
	public:
	FIFOBuffer<PacketUnion, 5> queue; //28*32+8 = 904
};

Mover mover;

//=========================================================================================
void send_wrong_crc()
{
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
	
	int size4 = (size+3)/4; //копируем сразу по 4 байта
	int *src = (int*)packet;
	int *dst = (int*)&mover.queue.End();
	for(int i=0; i<size4; ++i)
		dst[i] = src[i];
	mover.queue.Push();
}
