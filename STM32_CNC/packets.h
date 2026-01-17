#pragma once
// пакеты, используемые для связи

#include "stm32f4xx.h"
#include "common.h"
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
	DeviceCommand_SET_VEL_ACC,      //in
	DeviceCommand_SET_SWITCHES,     //in
	DeviceCommand_SET_SPINDLE_PARAMS,//in
	DeviceCommand_SET_COORDS,       //in
	DeviceCommand_SET_FEED,         //in
	DeviceCommand_SET_FEED_MULT,    //in
	DeviceCommand_SET_FEED_MODE,    //in
	DeviceCommand_SET_PWM,          //in
	DeviceCommand_SET_PWM_FREQ,     //in
	DeviceCommand_SET_STEP_SIZE,    //in
	DeviceCommand_SERVICE_COORDS,   //out
	DeviceCommand_TEXT_MESSAGE,     //out
	DeviceCommand_SERVICE_COMMAND,  //out
	DeviceCommand_SET_FRACT,        //in
	DeviceCommand_PAUSE,            //in
	DeviceCommand_BREAK,            //in
};
enum MoveMode //:char режим движения/интерполяции
{
	MoveMode_LINEAR = 0, //обычное движение по прямой
	MoveMode_HOME,       //наезд на дом
	MoveMode_FAST,       //быстрое перемещение по прямой
};
enum SwitchGroup //:char
{
	SwitchGroup_MIN = 0,
	SwitchGroup_MAX,
	SwitchGroup_HOME,
};
enum FeedType //:char
{
	FeedType_NORMAL = 0, //движение с заданной скоростью
	FeedType_ADC, //управление скоростью через АЦП
	FeedType_PER_REV, //стабилизация подачи на оборот (стабильный размер стружки/нагрузка на фрезу)
	FeedType_STABLE_REV, //стабилизация частоты оборотов (ограничение нагрева фрезы/стабильный кпд шпинделя)
	FeedType_SYNC, //синхронизация шпинделя с осью (нарезание резьбы и т.п.)
	FeedType_THROTTLING, //паузы при движении
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
	__packed int coord[MAX_AXES];
	char refCoord;
	float velocity;      //скорость подачи, мм/тик
	float acceleration;  //ускорение, мм/тик^2
	int uLength;         //длина в микронах
	float length;        // полная длина
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
	__packed int minCoord[MAX_AXES];
	__packed int maxCoord[MAX_AXES];
};
struct PacketSetVelAcc : public PacketCommon //осторожно, объявлено без packed
{
	float maxVelocity[MAX_AXES];
	float maxAcceleration[MAX_AXES];
};
struct PacketSetFeed : public PacketCommon
{
	float feedVel;
};
struct PacketSetFeedMult : public PacketCommon
{
	float feedMult;
};
struct PacketSetFeedMode : public PacketCommon
{
	FeedType mode;
};
struct PacketSetFeedStable : public PacketSetFeedMode
{
	float frequency;
};
struct PacketSetFeedPerRev : public PacketSetFeedMode
{
	float feedPerRev;
};
struct PacketSetFeedSync : public PacketSetFeedMode
{
	float step;
	int axeIndex;
	int pos;
};
struct PacketSetFeedThrottling : public PacketSetFeedMode
{
	bool enable;
	int period;
	int duration;
};
struct PacketSetFeedAdc : public PacketSetFeedMode
{
	bool enable;
};
struct PacketSetPWM : public PacketCommon
{
	char pin;
	float value;
};
struct PacketSetPWMFreq : public PacketCommon
{
	float freq;
	float slowFreq;
};
struct PacketSetStepSize : public PacketCommon
{
	__packed float stepSize[MAX_AXES];
};
struct PacketFract : public PacketCommon
{
};
struct PacketPause : public PacketCommon
{
	char needPause;
};
struct PacketSetSwitches : public PacketCommon
{
	char group;
	char pins[MAX_AXES];
	int16_t polarity;
};
struct PacketSetSpindleParams : public PacketCommon
{
	char pin;
	char marksCount;
	char frequency;
	char filterSize;
};
struct PacketSetCoords : public PacketCommon
{
	int coord[MAX_AXES];
	int16_t used;
};

//принимаемые от мк пакеты-----------------------------------------------------
struct PacketReceived //сообщение о том, что пакет принят
{
	DeviceCommand command;
	PacketCount packetNumber; //номер принятого пакета
	int8_t queue;
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
	__packed int coords[MAX_AXES];
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
	char p10[sizeof(PacketFract)];
	char p11[sizeof(PacketPause)];
	char p12[sizeof(PacketSetSwitches)];
	char p13[sizeof(PacketSetCoords)];
};

//размер структуры выровнен на 4, чтобы быстро копировать int[]
struct MaxPacket
{
	PacketUnion packet;
	int fill; //влияет на выравнивание и занимает правильный размер под црц, хранит флаг, что элемент заполнен
};
//=========================================================================================
uint32_t calc_crc(char *buffer, int size)
{
	//__disable_irq();
	volatile CRC_TypeDef *calc = CRC;
	calc->CR = CRC_CR_RESET;
	__NOP();//__NOP();__NOP();
	uint32_t *wordBuffer = (uint32_t*) buffer;
	
	while(size >= 4)
	{
		calc->DR = *(wordBuffer++);
		size -= 4;
	}
	
	uint8_t *buf = (uint8_t*)wordBuffer;
	while(size-- > 0)
	{
		calc->DR = *(buf++);
	}

	uint32_t r = calc->DR;
	//__enable_irq();
	return r;
}
