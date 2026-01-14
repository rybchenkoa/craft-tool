#pragma once

#include "Coords.h"
#include "RemoteDevice.h"

using PacketCount = char;

enum DeviceCommand:char //какие команды получает устройство
{
	DeviceCommand_MOVE = 1,
	DeviceCommand_WAIT,
	DeviceCommand_MOVE_MODE,
	DeviceCommand_PACKET_RECEIVED,     //пакет принят
	DeviceCommand_PACKET_REPEAT,       //пакет повторился
	DeviceCommand_QUEUE_FULL,          //очередь заполнена
	DeviceCommand_PACKET_ERROR_CRC,    //пакет пришёл побитым
	DeviceCommand_RESET_PACKET_NUMBER, //сбросить очередь пакетов
	DeviceCommand_ERROR_PACKET_NUMBER, //пришёл пакет с неправильным номером
	DeviceCommand_SET_VEL_ACC,
	DeviceCommand_SET_SWITCHES,
	DeviceCommand_SET_SPINDLE_PARAMS,
	DeviceCommand_SET_COORDS,
	DeviceCommand_SET_FEED, //выпилить?
	DeviceCommand_SET_FEED_MULT,
	DeviceCommand_SET_FEED_MODE,
	DeviceCommand_SET_PWM,
	DeviceCommand_SET_PWM_FREQ,
	DeviceCommand_SET_STEP_SIZE,
	DeviceCommand_SERVICE_COORDS,
	DeviceCommand_TEXT_MESSAGE,
	DeviceCommand_SERVICE_COMMAND,
	DeviceCommand_SET_FRACT,
	DeviceCommand_PAUSE,
	DeviceCommand_BREAK,
};

enum FeedType:char
{
	FeedType_NORMAL = 0, //движение с заданной скоростью
	FeedType_ADC, //управление скоростью через АЦП
	FeedType_PER_REV, //стабилизация подачи на оборот (стабильный размер стружки/нагрузка на фрезу)
	FeedType_STABLE_REV, //стабилизация частоты оборотов (ограничение нагрева фрезы/стабильный кпд шпинделя)
	FeedType_SYNC, //синхронизация шпинделя с осью (нарезание резьбы и т.п.)
	FeedType_THROTTLING, //паузы при движении
};

#pragma pack(push, 1)
struct PacketCommon
{
	char size;
	DeviceCommand command;
	PacketCount packetNumber;
};

struct PacketMove : public PacketCommon
{
	int coord[MAX_AXES];
	char refCoord;
	float velocity;      //скорость перемещения
	float acceleration;  //ускорение, шагов/тик^2
	int uLength;           //длина в микронах
	float length;        //полная длина
	int crc;
};

struct PacketWait : public PacketCommon
{
	int delay;
	int crc;
};

struct PacketInterpolationMode : public PacketCommon
{
	MoveMode mode;
	int crc;
};

struct PacketResetPacketNumber : public PacketCommon //сообщение о том, что пакет принят
{
	int crc;
};

struct PacketSetBounds : public PacketCommon //задать максимальные координаты
{
	int minCoord[MAX_AXES];
	int maxCoord[MAX_AXES];
	int crc;
};

struct PacketSetVelAcc : public PacketCommon //задать ускорение и скорость
{
	float maxVelocity[MAX_AXES];
	float maxAcceleration[MAX_AXES];
	int crc;
};

struct PacketSetFeed : public PacketCommon //задать подачу
{
	float feed;
	int crc;
};

struct PacketSetFeedMult : public PacketCommon //задать подачу
{
	float feedMult;
	int crc;
};

struct PacketSetFeedMode : public PacketCommon //настройки подачи
{
	FeedType mode;
};

struct PacketSetFeedNormal : public PacketSetFeedMode
{
	int crc;
};

struct PacketSetFeedPerRev : public PacketSetFeedMode
{
	float feedPerRev;
	int crc;
};

struct PacketSetFeedStable : public PacketSetFeedMode
{
	float frequency;
	int crc;
};

struct PacketSetFeedSync : public PacketSetFeedMode
{
	float step;
	int axeIndex;
	int pos;
	int crc;
};

struct PacketSetFeedThrottling : public PacketSetFeedMode
{
	bool enable;
	int period;
	int duration;
	int crc;
};

struct PacketSetFeedAdc : public PacketSetFeedMode
{
	bool enable;
	int crc;
};

struct PacketSetPWM : public PacketCommon //управление шим выходами
{
	char pin;
	float value;
	int crc;
};

struct PacketSetPWMFreq : public PacketCommon //управление шим выходами
{
	float freq;
	float slowFreq;
	int crc;
};

struct PacketSetStepSize : public PacketCommon
{
	float stepSize[MAX_AXES];
	int crc;
};

struct PacketFract : public PacketCommon
{
	int crc;
};

struct PacketPause : public PacketCommon
{
	char needStop;
	int crc;
};

struct PacketBreak : public PacketCommon
{
	int crc;
};

struct PacketSetSwitches : public PacketCommon
{
	char group;
	char pins[MAX_AXES];
	int16_t polarity;
	int crc;
};

struct PacketSetSpindleParams : public PacketCommon
{
	char pin;
	char marksCount;
	char frequency;
	char filterSize;
	int crc;
};

struct PacketSetCoords : public PacketCommon
{
	int coord[MAX_AXES];
	int16_t used;
	int crc;
};

//принимаемые от мк пакеты
struct PacketReceived //сообщение о том, что пакет принят
{
	DeviceCommand command;
	PacketCount packetNumber; //номер принятого пакета
	int8_t queue;
	int crc;
};

struct PacketErrorCrc //сообщение о том, что пакет принят
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

struct PacketServiceCoords //текущие координаты устройства
{
	DeviceCommand command;
	int coords[MAX_AXES];
	int crc;
};

struct PacketServiceCommand //текущий исполняемый устройством пакет
{
	DeviceCommand command;
	PacketCount packetNumber;
	int crc;
};
#pragma pack(pop)
