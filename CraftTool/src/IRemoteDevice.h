#pragma once
#include <list>
#include <queue>
#include "ComPortConnect.h"

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
    int coord[3];
    int crc;
};
struct PacketCircleMove
{
    char size;
    DeviceCommand command;
    PacketCount packetNumber;
    int coord[3];
    int circle[3];//I,J,K
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
    int minCoord[3];
    int maxCoord[3];
    int crc;
};
struct PacketSetVelAcc //задать ускорение и скорость
{
    char size;
    DeviceCommand command;
    PacketCount packetNumber;
    int maxrVelocity[3];
    int maxrAcceleration[3];
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
    float stepSize[3];
    int crc;
};
struct PacketSetVoltage
{
    char size;
    DeviceCommand command;
    PacketCount packetNumber;
    int voltage[3];
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
    virtual void set_bounds(double rMin[3], double rMax[3])=0; //границы координат
    virtual void set_velocity_and_acceleration(double velocity[3], double acceleration[3])=0; //задать скорость и ускорение
    virtual void set_feed(double feed)=0; //скорость подачи (скорость движения при резке)
    virtual void set_step_size(double stepSize[3])=0; //длина одного шага
    virtual bool need_next_command()=0; //есть ли ещё место в очереди команд
    virtual bool queue_empty() = 0; //есть ли ещё команды в очереди
};

//класс передаёт команды по com-порту на микроконтроллер, а он уже дальше интерполирует
class CRemoteDevice : public IRemoteDevice, public IPortToDevice
{
public:
    CRemoteDevice();

    void init();
    void reset_packet_queue();
    void set_move_mode(MoveMode mode);
    void set_plane(MovePlane plane);
    void set_position(double x, double y, double z);
    void set_circle_position(double x, double y, double z, double i, double j, double k);
    void wait(double time);
    void set_bounds(double rMin[3], double rMax[3]);
    void set_velocity_and_acceleration(double velocity[3], double acceleration[3]);
    void set_feed(double feed);
    void set_step_size(double stepSize[3]);
    void set_voltage(double voltage[3]);
    bool need_next_command();
    bool queue_empty();

    bool on_packet_received(char *data, int size);

    ComPortConnect *comPort;
    int missedSends; //пакет послан, ответ не получен
    int missedReceives; //принят битый пакет
    int missedHalfSend; //принято сообщение о битом пакете

    double scale[3];  //шагов на миллиметр
    double secToTick; //тиков таймера в одной секунде
    int subSteps;     //число делений шага на 2

protected:
    void init_crc();
    void make_crc(char *packet);
    unsigned crc32_stm32(unsigned init_crc, unsigned *buf, int len);

    template<typename T>
    void push_packet_common(T *packet);

    static DWORD WINAPI send_thread(void* __this);
    HANDLE hThread;

    std::queue<PacketCommon*> commandQueue;
    std::queue<char*> inputQueue;
    PacketCount packetNumber; //номер последнего добавленного пакета
    bool canSend; //пришло подтверждение о принятии пакета, можно слать следующий

    unsigned crc32Table[256];

};
