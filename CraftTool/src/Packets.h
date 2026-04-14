#pragma once

#include "Coords.h"
#include "RemoteDevice.h"

using PacketCount = char;

enum DeviceCommand:char //какие команды получает устройство
{
	DeviceCommand_RESET_PACKET_NUMBER = 1, // сбросить очередь пакетов
	DeviceCommand_PACKET_RECEIVED,     // * пакет принят
	DeviceCommand_PACKET_REPEAT,       // * пакет повторился
	DeviceCommand_QUEUE_FULL,          // * очередь заполнена
	DeviceCommand_PACKET_ERROR_CRC,    // * пакет пришёл побитым
	DeviceCommand_ERROR_PACKET_NUMBER, // * пришёл пакет с неправильным номером

	DeviceCommand_PAUSE,               // приостанавливает выполнение очереди
	DeviceCommand_BREAK,               // сбрасывает выполнение очереди пакетов

	DeviceCommand_TEXT_MESSAGE,        // * логи
	DeviceCommand_SERVICE_COMMAND,     // * какой пакет начал выполняться
	DeviceCommand_SERVICE_COORDS,      // * координаты устройства
	DeviceCommand_SERVICE_STATE,       // * состояние устройства

	DeviceCommand_SET_VEL_ACC,         // ограничения физики осей
	DeviceCommand_SET_STEP_SIZE,       // задать размер шагов
	DeviceCommand_SET_SWITCHES,        // концевики осей
	DeviceCommand_SET_SPINDLE_PARAMS,  // настройки датчика шпинделя
	DeviceCommand_SET_PWM_FREQ,        // задание настроек ШИМ

	DeviceCommand_MOVE,                // двигаться в заданную точку
	DeviceCommand_WAIT,                // ожидать миллисекунд
	DeviceCommand_MOVE_MODE,           // быстрое перемещение / рабоча подача
	DeviceCommand_SET_FRACT,           // конец траектории из нескольких отрезков
	DeviceCommand_SET_COORDS,          // задание аппаратных координат
	DeviceCommand_SET_FEED, //выпилить?
	DeviceCommand_SET_FEED_MULT,       // ручное управление подачей
	DeviceCommand_SET_FEED_MODE,       // синхронизация со шпинделем и т.п.
	DeviceCommand_SET_PWM,             // управление ШИМ пина
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

template <class T>
struct PacketTail : public T
{
	int crc;
};

struct PacketMove : public PacketCommon
{
	int coord[MAX_AXES];
	char refCoord;
	float velocity;      //скорость перемещения, мм/сек
	float acceleration;  //ускорение, мм/сек^2
	int uLength;           //длина в микронах
	float length;        //полная длина
};

struct PacketWait : public PacketCommon
{
	int delay;
};

struct PacketInterpolationMode : public PacketCommon
{
	MoveMode mode;
};

struct PacketResetPacketNumber : public PacketCommon //сообщение о том, что пакет принят
{
};

struct PacketSetBounds : public PacketCommon //задать максимальные координаты
{
	int minCoord[MAX_AXES];
	int maxCoord[MAX_AXES];
};

struct PacketSetVelAcc : public PacketCommon //задать ускорение и скорость
{
	float maxVelocity[MAX_AXES];
	float maxAcceleration[MAX_AXES];
};

struct PacketSetFeed : public PacketCommon //задать подачу
{
	float feed;
};

struct PacketSetFeedMult : public PacketCommon //задать подачу
{
	float feedMult;
};

struct PacketSetFeedMode : public PacketCommon //настройки подачи
{
	FeedType mode;
};

struct PacketSetFeedNormal : public PacketSetFeedMode
{
};

struct PacketSetFeedPerRev : public PacketSetFeedMode
{
	float feedPerRev;
};

struct PacketSetFeedStable : public PacketSetFeedMode
{
	float frequency;
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

struct PacketSetPWM : public PacketCommon //управление шим выходами
{
	char pin;
	float value;
};

struct PacketSetPWMFreq : public PacketCommon //управление шим выходами
{
	float freq;
	float slowFreq;
};

struct PacketSetStepSize : public PacketCommon
{
	float stepSize[MAX_AXES];
};

struct PacketFract : public PacketCommon
{
};

struct PacketPause : public PacketCommon
{
	char needStop;
};

struct PacketBreak : public PacketCommon
{
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

//принимаемые от мк пакеты
struct PacketReceived //сообщение о том, что пакет принят
{
	DeviceCommand command;
	PacketCount packetNumber; //номер принятого пакета
	int8_t queue;
};

struct PacketErrorCrc //сообщение о том, что пакет принят
{
	DeviceCommand command;
};

struct PacketErrorPacketNumber //сообщение о том, что сбилась очередь пакетов
{
	DeviceCommand command;
	PacketCount packetNumber;
};

struct PacketServiceCoords //текущие координаты устройства
{
	DeviceCommand command;
	int coords[MAX_AXES];
};

struct PacketServiceCommand //текущий исполняемый устройством пакет
{
	DeviceCommand command;
	PacketCount packetNumber;
};

struct PacketServiceState  //сообщение с текущим состоянием станка
{
	DeviceCommand command;
	int inputs;
	float velocity;
	float spindlePosition;
	float spindleVelocity;
};
#pragma pack(pop)
