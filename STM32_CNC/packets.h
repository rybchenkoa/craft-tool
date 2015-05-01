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
	DeviceCommand_PACKET_RECEIVED,  //out
	DeviceCommand_PACKET_REPEAT,    //out
	DeviceCommand_QUEUE_FULL,       //out
	DeviceCommand_PACKET_ERROR_CRC, //out
	DeviceCommand_RESET_PACKET_NUMBER,//in
	DeviceCommand_ERROR_PACKET_NUMBER,//out
	DeviceCommand_SET_BOUNDS,       //in
	DeviceCommand_SET_VEL_ACC,      //in
	DeviceCommand_SET_FEED,         //in
	DeviceCommand_SET_FEED_MULT,    //in
	DeviceCommand_SET_STEP_SIZE,    //in
	DeviceCommand_SET_VOLTAGE,      //in
	DeviceCommand_SERVICE_COORDS,   //out
	DeviceCommand_TEXT_MESSAGE,     //out
	DeviceCommand_SERVICE_COMMAND,  //out
	DeviceCommand_SET_FRACT,        //in
};
enum MoveMode //:char режим движения/интерполяции
{
	MoveMode_FAST = 0,
	MoveMode_LINEAR,
};

//--------------------------------------------------------------------
#pragma pack(push, 1)
struct PacketCommon
{
	DeviceCommand command;
	PacketCount packetNumber;
};
struct PacketMove : public PacketCommon
{
	int coord[NUM_COORDS];
	char refCoord;
	float16 velocity;      //скорость подачи, мм/тик
	float16 acceleration;  //ускорение, мм/тик^2
	int uLength;           //длина в микронах
	float16 invProj;       // полная длина / длина опорной координаты, мм/шаг
};
struct PacketWait : public PacketCommon
{
	int delay;
};
struct PacketInterpolationMode : public PacketCommon
{
	MoveMode mode;
};
struct PacketSetBounds : public PacketCommon
{
	int minCoord[NUM_COORDS];
	int maxCoord[NUM_COORDS];
};
struct PacketSetVelAcc : public PacketCommon
{
	float16 maxVelocity[NUM_COORDS];
	float16 maxAcceleration[NUM_COORDS];
};
struct PacketSetFeed : public PacketCommon
{
	float16 feedVel;
};
struct PacketSetFeedMult : public PacketCommon
{
	float16 feedMult;
};
struct PacketSetStepSize : public PacketCommon
{
	float stepSize[NUM_COORDS];
};
struct PacketSetVoltage : public PacketCommon
{
	int voltage[NUM_COORDS];
};
struct PacketFract : public PacketCommon
{
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
struct PacketServiceCoords  //сообщение с текущими координатами сверла
{
	DeviceCommand command;
	int coords[NUM_COORDS];
	int crc;
};
struct PacketServiceCommand  //сообщение с текущим исполняемым пакетом
{
	DeviceCommand command;
	PacketCount packetNumber;
	int crc;
};
#pragma pack(pop)

//=========================================================================================
//здесь только входящие пакеты
union PacketUnion
{
	char p0[sizeof(PacketCommon)];
	char p1[sizeof(PacketMove)];
	char p2[sizeof(PacketWait)];
	char p3[sizeof(PacketInterpolationMode)];
	char p4[sizeof(PacketSetBounds)];
	char p5[sizeof(PacketSetVelAcc)];
	char p6[sizeof(PacketSetFeed)];
	char p7[sizeof(PacketSetFeedMult)];
	char p8[sizeof(PacketSetStepSize)];
	char p9[sizeof(PacketSetVoltage)];
	char p10[sizeof(PacketFract)];
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
