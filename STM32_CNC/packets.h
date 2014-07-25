#include "stm32f10x.h"
#include "common.h"
#include "float16.h"
//=========================================================================================
typedef char PacketCount;
enum DeviceCommand //:char какие команды получает устройство
{
	DeviceCommand_MOVE = 1,         //in
	DeviceCommand_WAIT,             //in
	DeviceCommand_MOVE_MODE,        //in
	DeviceCommand_SET_PLANE,        //in
	DeviceCommand_PACKET_RECEIVED,  //out
	DeviceCommand_PACKET_ERROR_CRC, //out
	DeviceCommand_RESET_PACKET_NUMBER,//in
	DeviceCommand_ERROR_PACKET_NUMBER,//out
	DeviceCommand_SET_BOUNDS,       //in
	DeviceCommand_SET_VEL_ACC,      //in
	DeviceCommand_SET_FEED,         //in
	DeviceCommand_SET_STEP_SIZE,    //in
};
enum MoveMode //:char режим движения/интерполяции
{
	MoveMode_FAST = 0,
	MoveMode_LINEAR,
	MoveMode_CW_ARC,
	MoveMode_CCW_ARC,
};
enum MovePlane //:char плоскость интерполяции
{
	MovePlane_XY = 0,
	MovePlane_ZX,
	MovePlane_YZ,
};
//--------------------------------------------------------------------
#pragma pack(push, 1)
struct PacketCommon
{
	DeviceCommand command;
	PacketCount packetNumber;
};
struct PacketMove
{
	DeviceCommand command;
	PacketCount packetNumber;
	int coord[NUM_COORDS];
};
struct PacketCircleMove //30 байт
{
	DeviceCommand command;
	PacketCount packetNumber;
	int coord[NUM_COORDS];
	int circle[3];//I,J,K
};
struct PacketWait
{
	DeviceCommand command;
	PacketCount packetNumber;
	int delay;
	int crc;
};
struct PacketInterpolationMode
{
	DeviceCommand command;
	PacketCount packetNumber;
	MoveMode mode;
};
struct PacketSetPlane
{
	DeviceCommand command;
	PacketCount packetNumber;
	MovePlane plane;
};
struct PacketSetBounds
{
	DeviceCommand command;
	PacketCount packetNumber;
	int minCoord[NUM_COORDS];
	int maxCoord[NUM_COORDS];
};
struct PacketSetVelAcc
{
	DeviceCommand command;
	PacketCount packetNumber;
	int maxrVelocity[NUM_COORDS];
	int maxrAcceleration[NUM_COORDS];
};
struct PacketSetFeed
{
	DeviceCommand command;
	PacketCount packetNumber;
	int rFeed;
};
struct PacketSetStepSize
{
	DeviceCommand command;
	PacketCount packetNumber;
	float stepSize[NUM_COORDS];
};

//принимаемые от мк пакеты-----------------------------------------------------
struct PacketReceived //сообщение о том, что пакет принят
{
	DeviceCommand command;
	PacketCount packetNumber; //номер принятого пакета
	int crc;
};
struct PacketErrorCrc //сообщение о том, что пакет испортился при передаче
{
	DeviceCommand command;
	int crc;
};
struct PacketErrorPacketNumber //сообщение о том, что сбилась очередь пакетов
{
	DeviceCommand command;
	PacketCount packetNumber;
	int crc;
};
#pragma pack(pop)

//=========================================================================================
union PacketUnion
{
	PacketCommon common;
	PacketMove move;
	PacketCircleMove circleMove;
	PacketWait wait;
	PacketInterpolationMode interpolationMode;
	PacketSetPlane setPlane;
	PacketSetBounds setBounds;
	PacketSetVelAcc setVelAcc;
	PacketSetFeed setFeed;
	PacketSetStepSize setStepSize;
	int pack[7]; //для выравнивания, чтобы быстро копировать int[]
};

//=========================================================================================
uint32_t calc_crc(char *buffer, int size)
{
	volatile CRC_TypeDef *calc = CRC;
	calc->CR |= CRC_CR_RESET;
	
	uint32_t wordLength = size/4;
	uint32_t *wordBuffer = (uint32_t*) buffer;
	
	while(wordLength--)
	{
		calc->DR = *(wordBuffer++);
		__NOP();__NOP();__NOP();__NOP();
	}
	
	switch(size & 3)
	{
		case 1: calc->DR = (*wordBuffer) & 0x000000FF; __NOP();__NOP();__NOP();__NOP(); break;
		case 2: calc->DR = (*wordBuffer) & 0x0000FFFF; __NOP();__NOP();__NOP();__NOP(); break;
		case 3: calc->DR = (*wordBuffer) & 0x00FFFFFF; __NOP();__NOP();__NOP();__NOP(); break;
	}
		
	return calc->DR;
}
