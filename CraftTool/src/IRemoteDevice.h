#pragma once
#include <list>
#include <queue>
#include <memory>
#include <QObject>
#include "ComPortConnect.h"
#include "stdint.h"

#define NUM_COORDS 4 //сколько координат задаем в G-коде
#define MAX_AXES   5 //сколько всего есть осей на контроллере
#define MAX_IN_PINS 11 //количество входных пинов

typedef double coord;//чтобы не путаться, координатный тип введём отдельно

struct Coords   //все координаты устройства
{
    union
    {
        struct
        {
            coord x, y, z, a, b;
        };
        struct
        {
            coord r[MAX_AXES];
        };
    };

	Coords() { for (int i = 0; i < MAX_AXES; ++i) r[i] = 0; };
};

typedef char PacketCount;
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
enum MoveMode:char //режим движения/интерполяции
{
    MoveMode_LINEAR = 0,
	MoveMode_HOME,

	MoveMode_FAST,
};
enum SwitchGroup:char
{
	SwitchGroup_MIN = 0,
	SwitchGroup_MAX,
	SwitchGroup_HOME,
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
    int size;
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

//для теста надо будет сделать отдельную реализацию класса
class IRemoteDevice
{
public:
    virtual void init()=0; //сбрасываем очередь команд, ищем начало координат и т.п.
    virtual void reset_packet_queue()=0;
    virtual void set_move_mode(MoveMode mode)=0; //задаём режим интерполяции
    virtual void set_position(Coords position)=0; //перемещаем фрезу
    virtual void wait(double time)=0; //задержка
    virtual void set_velocity_and_acceleration(double velocity[MAX_AXES], double acceleration[MAX_AXES])=0; //задать скорость и ускорение
    virtual double get_max_velocity(int coord)=0;
    virtual double get_max_acceleration(int coord)=0;
    virtual void set_feed(double feed)=0; //скорость подачи (скорость движения при резке)
    virtual void set_feed_multiplier(double multiplier)=0; //множитель скорости подачи
    virtual void set_feed_normal()=0; //подача в мм/сек
    virtual void set_feed_per_rev(double feed)=0; //подача в мм/оборот
    virtual void set_feed_stable(double frequency)=0; //стабилизация оборотов шпинделя
    virtual void set_feed_sync(double step, double pos, int axe)=0; //синхронизация оси со шпинделем
    virtual void set_feed_throttling(bool enable, int period, int size)=0; //движение рывками
    virtual void set_feed_adc(bool enable)=0; //управление подачей через напряжение
	
	virtual void set_spindle_vel(double feed)=0; //скорость подачи (скорость движения при резке)
    virtual void set_step_size(double stepSize[MAX_AXES])=0; //длина одного шага
    virtual void pause_moving(bool needStop)=0; //временная остановка движения
    virtual void break_queue()=0; //полная остановка с прерыванием программы
	virtual void homing()=0; //отъехать к концевикам и задать машинные координаты

    virtual int  queue_size() = 0; //длина очереди команд

    virtual void set_current_line(int line)=0; //задаёт номер строки, для которой сейчас будут вызываться команды
    virtual int  get_current_line()=0; //возвращает номер строки, для которой сейчас исполняются команды
    virtual const Coords* get_current_coords()=0; //последние принятые координаты
    virtual double get_min_step(int axis1, int axis2)=0; //точность устройства
};

//класс передаёт команды по com-порту на микроконтроллер, а он уже дальше интерполирует
class CRemoteDevice : public QObject, public IRemoteDevice
{
    Q_OBJECT
public:
    CRemoteDevice();
    ~CRemoteDevice();

    void init() override;
    void reset_packet_queue() override;
    void set_move_mode(MoveMode mode) override;
    void set_position(Coords position) override;
    void wait(double time) override;
    void set_velocity_and_acceleration(double velocity[MAX_AXES], double acceleration[MAX_AXES]) override;
    double get_max_velocity(int coord) override;
    double get_max_acceleration(int coord) override;
    void set_feed(double feed) override;
    void set_feed_multiplier(double multiplier) override;
    void set_feed_normal() override;
    void set_feed_per_rev(double feed) override;
    void set_feed_stable(double frequency) override;
    void set_feed_sync(double step, double pos, int axe) override;
    void set_feed_throttling(bool enable, int period, int size) override;
    void set_feed_adc(bool enable) override;
    void set_step_size(double stepSize[MAX_AXES]) override;
	void set_spindle_vel(double feed) override;
    void pause_moving(bool needStop) override;
    void break_queue() override;
	void homing() override;

    void set_fract();

    int  queue_size() override;

    void set_current_line(int line) override;
    int  get_current_line() override;
    const Coords* get_current_coords() override;
    double get_min_step(int axis1, int axis2) override;
    int send_lag_ms();

    bool on_packet_received(char *data, int size);
    bool process_packet(char *data, int size);

	bool inited;                       //порт нужен для инициализации, но до инициализации нельзя принимать с него лишних данных
    ComPortConnect *comPort;           //подключение к удалённому устройству
    int missedSends;                   //пакет послан, ответ не получен
    int missedReceives;                //принят битый пакет
    int missedHalfSend;                //принято сообщение о битом пакете
    int packSends;                     //послано пакетов

    //текущее состояние
    double scale[MAX_AXES];            //шагов на миллиметр
	double stepSize[MAX_AXES];         //длина одного шага
    double minStep;                    //макс. точность устройства
    double secToTick;                  //тиков таймера в одной секунде
    Coords lastPosition;               //последняя переданная позиция
    Coords lastDelta;                  //последний вектор сдвига
    double feed;                       //подача
	double spindleSpeed;               //скорость шпинделя
    MoveMode moveMode;                 //режим перемещения
    double velocity[MAX_AXES];         //максимальная скорость по каждой оси
    double acceleration[MAX_AXES];     //максимальное ускорение по каждой оси
    double fractValue;                 //насколько должна измениться траектория, чтобы считать её новой линией
    bool fractSended;                  //послан ли уже излом траектории
	bool usedCoords[MAX_AXES];         //используемые интерпретатором координаты
	bool usedAxes[MAX_AXES];           //используемые станком оси
	int  slaveAxes[MAX_AXES];          //связанная ось (к одной оси можно привязать еще только одну)
	std::vector<int> toDeviceIndex;    //перевод номеров координат в номера генераторов сигнала на устройстве
	std::vector<int> fromDeviceIndex;  //обратное преобразование при получении координат с устройства
	bool invertAxe[MAX_AXES];          //инвертировать ли ось
	int switchPolarity;                //биты - значения активного уровня сигнала
	int  switchMin[MAX_AXES];          //концевик на минимум
	int  switchMax[MAX_AXES];          //концевик на максимум
	int  switchHome[MAX_AXES];         //концевик для дома
	double backHome[MAX_AXES];         //на сколько отъехать от дома
	Coords coordHome;                  //какие задать координаты дому
	std::string homingScript;          //как ехать к дому

    //состояние удалённого устройства
    Coords currentCoords;              //текущие координаты

protected:
	void set_switches(SwitchGroup group, int pins[MAX_AXES]);
	void set_coord(Coords pos, bool used[MAX_AXES]);
	void set_pwm_freq(double fast, double slow);

    template<typename T>
    void push_packet_common(T *packet);

    template<typename T>
    void push_packet_modal(T *packet);

    static DWORD WINAPI send_thread(void* _this);
    HANDLE hThread;

    struct ConnectData;
    std::unique_ptr<ConnectData> commands[2]; //очередь для g-кода и для сервисных команд
    CRITICAL_SECTION queueCS;     //защита очереди от порчи
    HANDLE eventQueueAdd;         //в очередь добавлен пакет
    HANDLE eventPacketReceived;   //сообщение о принятии пакета

    int pushLine;                 //строка, из которой читаются команды
    int workLine;                 //строка, команда которой сейчас выполняется

signals:
    void coords_changed(float x, float y, float z);

};
