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
	DeviceCommand_SET_VOLTAGE,      //in
	DeviceCommand_SERVICE_COORDS,   //out
	DeviceCommand_TEXT_MESSAGE,     //out
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
struct PacketCircleMove
{
	DeviceCommand command;
	PacketCount packetNumber;
	int coord[NUM_COORDS];
	int center[NUM_COORDS];
};
struct PacketWait
{
	DeviceCommand command;
	PacketCount packetNumber;
	int delay;
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
struct PacketSetVoltage
{
	DeviceCommand command;
	PacketCount packetNumber;
	int voltage[NUM_COORDS];
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
struct PacketServiceCoords
{
	DeviceCommand command;
	int coords[NUM_COORDS];
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
};

//размер структуры выровнен на 4, чтобы быстро копировать int[]
struct MaxPacket
{
	PacketUnion packet;
	int dummyCrc; //влияет на выравнивание и занимает правильный размер под црц
};
//=========================================================================================
uint32_t calc_crc(char *buffer, int size)
{
	//__disable_irq();
	volatile CRC_TypeDef *calc = CRC;
	calc->CR |= CRC_CR_RESET;
	//__NOP();__NOP();__NOP();
	uint32_t *wordBuffer = (uint32_t*) buffer;
	
	while(size >= 4)
	{
		calc->DR = *(wordBuffer++);
		size -= 4;
	}
	
	char *buf = (char*)wordBuffer;
	while(size-- > 0)
	{
		calc->DR = *(buf++);
	}

	uint32_t r = calc->DR;
	//__enable_irq();
	return r;
}
