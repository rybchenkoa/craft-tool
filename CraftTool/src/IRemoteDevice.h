#pragma once
#include <list>
#include <queue>
#include <QObject>
#include "ComPortConnect.h"

#define NUM_COORDS 3

typedef char PacketCount;
enum DeviceCommand:char //какие команды получает устройство
{
    DeviceCommand_MOVE = 1,
    DeviceCommand_WAIT,
    DeviceCommand_MOVE_MODE,
    DeviceCommand_SET_PLANE,
    DeviceCommand_PACKET_RECEIVED,
    DeviceCommand_PACKET_ERROR_CRC,
    DeviceCommand_RESET_PACKET_NUMBER,
    DeviceCommand_ERROR_PACKET_NUMBER,
    DeviceCommand_SET_BOUNDS,
    DeviceCommand_SET_VEL_ACC,
    DeviceCommand_SET_FEED,
    DeviceCommand_SET_STEP_SIZE,
    DeviceCommand_SET_VOLTAGE,
    DeviceCommand_SERVICE_COORDS,
    DeviceCommand_TEXT_MESSAGE,
    DeviceCommand_SERVICE_COMMAND,
};
enum MoveMode:char //режим движения/интерполяции
{
    MoveMode_FAST = 0,
    MoveMode_LINEAR,
    MoveMode_CW_ARC,
    MoveMode_CCW_ARC,
};
enum MovePlane:char //плоскость интерполяции
{
    MovePlane_XY = 0,
    MovePlane_ZX,
    MovePlane_YZ,
};
#pragma pack(push, 1)
struct PacketCommon
{
    char size;
    DeviceCommand command;
    PacketCount packetNumber;
};
struct PacketMove
{
    char size;
    DeviceCommand command;
    PacketCount packetNumber;
    int coord[NUM_COORDS];
    int crc;
};
struct PacketCircleMove
{
    char size;
    DeviceCommand command;
    PacketCount packetNumber;
    int coord[NUM_COORDS];
    int circle[NUM_COORDS];//I,J,K
    int crc;
};
struct PacketWait
{
    char size;
    DeviceCommand command;
    PacketCount packetNumber;
    int delay;
    int crc;
};
struct PacketInterpolationMode
{
    char size;
    DeviceCommand command;
    PacketCount packetNumber;
    MoveMode mode;
    int crc;
};
struct PacketSetPlane
{
    char size;
    DeviceCommand command;
    PacketCount packetNumber;
    MovePlane plane;
    int crc;
};
struct PacketResetPacketNumber //сообщение о том, что пакет принят
{
    char size;
    DeviceCommand command;
    PacketCount packetNumber;
    int crc;
};
struct PacketSetBounds //задать максимальные координаты
{
    char size;
    DeviceCommand command;
    PacketCount packetNumber;
    int minCoord[NUM_COORDS];
    int maxCoord[NUM_COORDS];
    int crc;
};
struct PacketSetVelAcc //задать ускорение и скорость
{
    char size;
    DeviceCommand command;
    PacketCount packetNumber;
    int maxrVelocity[NUM_COORDS];
    int maxrAcceleration[NUM_COORDS];
    int crc;
};
struct PacketSetFeed //задать подачу
{
    char size;
    DeviceCommand command;
    PacketCount packetNumber;
    int rFeed;
    int crc;
};
struct PacketSetStepSize
{
    char size;
    DeviceCommand command;
    PacketCount packetNumber;
    float stepSize[NUM_COORDS];
    int crc;
};
struct PacketSetVoltage
{
    char size;
    DeviceCommand command;
    PacketCount packetNumber;
    int voltage[NUM_COORDS];
    int crc;
};

//принимаемые от мк пакеты
struct PacketReceived //сообщение о том, что пакет принят
{
    DeviceCommand command;
    PacketCount packetNumber; //номер принятого пакета
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
    int coords[NUM_COORDS];
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
    virtual void set_plane(MovePlane plane)=0; //задаём плоскость интерполяции
    virtual void set_position(double x, double y, double z)=0; //перемещаем фрезу
    virtual void set_circle_position(double x, double y, double z, double i, double j, double k)=0; //перемещаем по дуге
    virtual void wait(double time)=0; //задержка
    virtual void set_bounds(double rMin[NUM_COORDS], double rMax[NUM_COORDS])=0; //границы координат
    virtual void set_velocity_and_acceleration(double velocity[NUM_COORDS], double acceleration[NUM_COORDS])=0; //задать скорость и ускорение
    virtual void set_feed(double feed)=0; //скорость подачи (скорость движения при резке)
    virtual void set_step_size(double stepSize[NUM_COORDS])=0; //длина одного шага

    virtual bool queue_empty() = 0; //есть ли ещё команды в очереди

    virtual void set_current_line(int line)=0; //задаёт номер строки, для которой сейчас будут вызываться команды
    virtual int  get_current_line()=0; //возвращает номер строки, для которой сейчас исполняются команды
};

//класс передаёт команды по com-порту на микроконтроллер, а он уже дальше интерполирует
class CRemoteDevice : public QObject, public IRemoteDevice, public IPortToDevice
{
    Q_OBJECT
public:
    CRemoteDevice();
    ~CRemoteDevice();

    void init() override;
    void reset_packet_queue() override;
    void set_move_mode(MoveMode mode) override;
    void set_plane(MovePlane plane) override;
    void set_position(double x, double y, double z) override;
    void set_circle_position(double x, double y, double z, double i, double j, double k) override;
    void wait(double time) override;
    void set_bounds(double rMin[NUM_COORDS], double rMax[NUM_COORDS]) override;
    void set_velocity_and_acceleration(double velocity[NUM_COORDS], double acceleration[NUM_COORDS]) override;
    void set_feed(double feed) override;
    void set_step_size(double stepSize[NUM_COORDS]) override;

    void set_voltage(double voltage[NUM_COORDS]);

    bool queue_empty() override;

    virtual void set_current_line(int line) override;
    virtual int  get_current_line() override;

    bool on_packet_received(char *data, int size);

    bool process_packet(char *data, int size);

    ComPortConnect *comPort;           //подключение к удалённому устройству
    int missedSends;                   //пакет послан, ответ не получен
    int missedReceives;                //принят битый пакет
    int missedHalfSend;                //принято сообщение о битом пакете

    double scale[NUM_COORDS];          //шагов на миллиметр
    double secToTick;                  //тиков таймера в одной секунде
    int subSteps;                      //число делений шага на 2
    double currentCoords[NUM_COORDS];  //текущие координаты на удалённом устройстве

protected:
    void init_crc();
    void make_crc(char *packet);
    unsigned crc32_stm32(unsigned init_crc, unsigned *buf, int len);

    template<typename T>
    void push_packet_common(T *packet);

    static DWORD WINAPI send_thread(void* _this);
    HANDLE hThread;

    std::queue<PacketCommon*> commandQueue;
    CRITICAL_SECTION queueCS;     //защита очереди от порчи
    HANDLE eventQueueAdd;         //в очередь добавлен пакет
    HANDLE eventPacketReceived;   //сообщение о принятии пакета
    PacketCount packetNumber;     //номер последнего добавленного пакета

    int pushLine;                 //строка, из которой читаются команды
    int workLine;                 //строка, команда которой сейчас выполняется
    struct WorkPacket
    {
        PacketCount packet;
        int line;
    };
    std::queue<WorkPacket> workQueue; //посланные устройству пакеты, которые ещё не исполнены

    unsigned crc32Table[256];

signals:
    void coords_changed(float x, float y, float z);

};
