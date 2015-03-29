#include "IRemoteDevice.h"
#include "log.h"
#include "AutoLockCS.h"
#include "config_defines.h"


//============================================================
CRemoteDevice::CRemoteDevice()
{
    init_crc();
    packetNumber = -1;
    missedSends = 0;
    missedReceives = 0;
    missedHalfSend = 0;
    pushLine = -1;
    workLine = -1;

    InitializeCriticalSection(&queueCS);
    eventQueueAdd = CreateEvent(nullptr, false, false, nullptr);
    eventPacketReceived = CreateEvent(nullptr, false, false, nullptr);
    DWORD   threadId;
    hThread = CreateThread(NULL, 0, send_thread, this, 0, &threadId);
    comPort = nullptr;
}

//============================================================
CRemoteDevice::~CRemoteDevice()
{
    TerminateThread(hThread, 0);
    CloseHandle(eventPacketReceived);
    CloseHandle(eventQueueAdd);
    DeleteCriticalSection(&queueCS);
}

//============================================================
#define CRC32_POLY 0x04C11DB7
void CRemoteDevice::init_crc()
{
    unsigned crc;
    for (int i = 0; i < 256; ++i)
    {
        crc = i << 24;
        for (int j = 8; j > 0; --j)
            crc = crc & 0x80000000 ? (crc << 1) ^ CRC32_POLY : (crc << 1);
        crc32Table[i] = crc;
    }
}

//============================================================
unsigned CRemoteDevice::crc32_stm32(unsigned init_crc, unsigned *buf, int len)
{
    unsigned v;
    unsigned crc;
    crc = init_crc;
    while(len >= 4)
    {
        v = htonl(*buf++);
        crc = ( crc << 8 ) ^ crc32Table[0xFF & ( (crc >> 24) ^ (v ) )];
        crc = ( crc << 8 ) ^ crc32Table[0xFF & ( (crc >> 24) ^ (v >> 8) )];
        crc = ( crc << 8 ) ^ crc32Table[0xFF & ( (crc >> 24) ^ (v >> 16) )];
        crc = ( crc << 8 ) ^ crc32Table[0xFF & ( (crc >> 24) ^ (v >> 24) )];
        len -= 4;
    }

    char *cbuf = (char*)buf;
    while(len-- != 0)
    {
        v = htonl(*cbuf++);
        crc = ( crc << 8 ) ^ crc32Table[0xFF & ( (crc >> 24) ^ (v ) )];
        crc = ( crc << 8 ) ^ crc32Table[0xFF & ( (crc >> 24) ^ (v >> 8) )];
        crc = ( crc << 8 ) ^ crc32Table[0xFF & ( (crc >> 24) ^ (v >> 16) )];
        crc = ( crc << 8 ) ^ crc32Table[0xFF & ( (crc >> 24) ^ (v >> 24) )];
    }
    return crc;
}

//============================================================
template<typename T>
void CRemoteDevice::push_packet_common(T *packet)
{
    AutoLockCS lock(queueCS);

    packet->size = sizeof(*packet);
    packet->packetNumber = packetNumber++;
    make_crc((char*)packet);
    commandQueue.push((PacketCommon*)packet);

    WorkPacket work;
    work.line = pushLine;
    work.packet = packet->packetNumber;
    workQueue.push(work);

    SetEvent(eventQueueAdd);
}

//============================================================
void CRemoteDevice::set_move_mode(MoveMode mode)
{
    auto packet = new PacketInterpolationMode;
    packet->command = DeviceCommand_MOVE_MODE; //сменить режим перемещения
    packet->mode = mode; //новый режим
    push_packet_common(packet);
}

//============================================================
void CRemoteDevice::set_plane(MovePlane plane)
{
    auto packet = new PacketSetPlane;
    packet->command = DeviceCommand_SET_PLANE; //сменить плоскость
    packet->plane = plane; //новая плоскость
    push_packet_common(packet);
}

//============================================================
void CRemoteDevice::set_position(double x, double y, double z)
{
    auto packet = new PacketMove;
    packet->command = DeviceCommand_MOVE; //двигаться
    packet->coord[0] = int(x*scale[0]);
    packet->coord[1] = int(y*scale[1]);
    packet->coord[2] = int(z*scale[2]);
    //log_message("   GO TO %d, %d, %d\n", packet->coord[0], packet->coord[1], packet->coord[2]);
    push_packet_common(packet);
}

//============================================================
void CRemoteDevice::set_circle_position(double x, double y, double z, double i, double j, double k)
{
    auto packet = new PacketCircleMove;
    packet->command = DeviceCommand_MOVE; //число координат определяется по текущему режиму перемещения
    packet->coord[0] = int(x*scale[0]);
    packet->coord[1] = int(y*scale[1]);
    packet->coord[2] = int(z*scale[2]);
    packet->circle[0] = int(i*scale[0]);
    packet->circle[1] = int(j*scale[1]);
    packet->circle[2] = int(k*scale[2]);
    push_packet_common(packet);
}

//============================================================
void CRemoteDevice::wait(double time)
{
    auto packet = new PacketWait;
    packet->command = DeviceCommand_WAIT;
    packet->delay = int(time*1000); //задержка
    push_packet_common(packet);
}

//============================================================
void CRemoteDevice::set_bounds(double rMin[NUM_COORDS], double rMax[NUM_COORDS])
{
    auto packet = new PacketSetBounds;
    packet->command = DeviceCommand_SET_BOUNDS;
    for(int i = 0; i < NUM_COORDS; ++i)
    {
        packet->minCoord[i] = int(rMin[i]*scale[i]);
        packet->maxCoord[i] = int(rMax[i]*scale[i]);
    }
    push_packet_common(packet);
}

//============================================================
//мм/сек, мм/сек^2
//переводим в шаг/мкс, шаг/мкс^2
//значения хранятся в виде 1.0/value, поскольку в int
void CRemoteDevice::set_velocity_and_acceleration(double velocity[NUM_COORDS], double acceleration[NUM_COORDS])
{
    auto packet = new PacketSetVelAcc;
    packet->command = DeviceCommand_SET_VEL_ACC; //задать макс скорость и ускорение
    for(int i = 0; i < NUM_COORDS; ++i)
    {
        packet->maxrVelocity[i] = int(1.0/(velocity[i]*scale[i]/secToTick));
        packet->maxrAcceleration[i] = int(1.0/(acceleration[i]*scale[i]/(secToTick*secToTick)));
    }
    push_packet_common(packet);
}

//============================================================
//мм/сек
void CRemoteDevice::set_feed(double feed)
{
    auto packet = new PacketSetFeed;
    packet->command = DeviceCommand_SET_FEED;
    packet->rFeed = int(1.0/(feed/secToTick));
    push_packet_common(packet);
}

//============================================================
void CRemoteDevice::set_step_size(double stepSize[NUM_COORDS])
{
    auto packet = new PacketSetStepSize;
    packet->command = DeviceCommand_SET_STEP_SIZE; //задать макс скорость и ускорение
    for(int i = 0; i < NUM_COORDS; ++i)
    {
        packet->stepSize[i] = float(stepSize[i]);
    }
    push_packet_common(packet);
}

//============================================================
void CRemoteDevice::set_voltage(double voltage[NUM_COORDS])
{
    auto packet = new PacketSetVoltage;
    packet->command = DeviceCommand_SET_VOLTAGE;
    for(int i = 0; i < NUM_COORDS; ++i)
    {
        packet->voltage[i] = int(voltage[i]*255);
    }
    push_packet_common(packet);
}

//============================================================
void CRemoteDevice::reset_packet_queue()
{
    auto packet = new PacketResetPacketNumber;
    packet->command = DeviceCommand_RESET_PACKET_NUMBER;
    push_packet_common(packet);
}

//============================================================
void CRemoteDevice::init()
{
    packetNumber = -1;
    reset_packet_queue();

    auto try_get_float = [/*&g_config*/](const char *key) -> float
    {
        float value;
        if(!g_config->get_float(key, value))
            throw(std::string("key not found in konfig: ") + key);
        return value;
    };

    secToTick = 1000000.0;
    subSteps = 2;
    double stepsPerMm[NUM_COORDS];
    stepsPerMm[0] = try_get_float(CFG_STEPS_PER_MM "X");
    stepsPerMm[1] = try_get_float(CFG_STEPS_PER_MM "Y");
    stepsPerMm[2] = try_get_float(CFG_STEPS_PER_MM "Z");

    scale[0] = stepsPerMm[0]*(1<<subSteps);
    scale[1] = stepsPerMm[1]*(1<<subSteps);
    scale[2] = stepsPerMm[2]*(1<<subSteps);

    double maxVelocity[NUM_COORDS];// = {1000.0, 1000.0, 1000.0};
    maxVelocity[0] = try_get_float(CFG_MAX_VELOCITY "X");
    maxVelocity[1] = try_get_float(CFG_MAX_VELOCITY "Y");
    maxVelocity[2] = try_get_float(CFG_MAX_VELOCITY "Z");

    double maxAcceleration[NUM_COORDS];// = {50.0, 50.0, 50.0};
    maxAcceleration[0] = try_get_float(CFG_MAX_ACCELERATION "X");
    maxAcceleration[1] = try_get_float(CFG_MAX_ACCELERATION "Y");
    maxAcceleration[2] = try_get_float(CFG_MAX_ACCELERATION "Z");

    double voltage[NUM_COORDS]; // = {0.7, 0.7, 0.7};
    voltage[0] = try_get_float(CFG_MAX_VOLTAGE "X");
    voltage[1] = try_get_float(CFG_MAX_VOLTAGE "Y");
    voltage[2] = try_get_float(CFG_MAX_VOLTAGE "Z");

    double feed = 10.0;
    double stepSize[NUM_COORDS] = {1.0/scale[0], 1.0/scale[1], 1.0/scale[2]};

    set_velocity_and_acceleration(maxVelocity, maxAcceleration);
    set_feed(feed);
    set_step_size(stepSize);
    set_voltage(voltage);
}

//============================================================
void CRemoteDevice::make_crc(char *packet)
{
    int size = *packet-1;
    char *buffer = packet+1;
    int &crc = *(int*)(buffer+size-4);

    crc = crc32_stm32(0xFFFFFFFF, (unsigned int*)(buffer), size-4);
}

//============================================================
void CRemoteDevice::set_current_line(int line)
{
    pushLine = line;
}

//============================================================
int CRemoteDevice::get_current_line()
{
    return workLine;
}

//============================================================
const double* CRemoteDevice::get_current_coords()
{
    return currentCoords;
}

//============================================================
int CRemoteDevice::queue_size()
{
    AutoLockCS lock(queueCS);
    return commandQueue.size();
}

//============================================================
bool CRemoteDevice::process_packet(char *data, int size)
{
    switch(*data)
    {
    case DeviceCommand_SERVICE_COORDS:
    {
        PacketServiceCoords *packet = (PacketServiceCoords*) data;
        for(int i = 0; i < NUM_COORDS; ++i)
            currentCoords[i] = packet->coords[i] / scale[i];
        emit coords_changed(currentCoords[0], currentCoords[1], currentCoords[2]);
        //log_message("eto ono (%d, %d, %d)\n", packet->coords[0], packet->coords[1], packet->coords[2]);
        return true;
    }
    case DeviceCommand_SERVICE_COMMAND:
    {
        PacketServiceCommand *packet = (PacketServiceCommand*) data; //находим строку, которая сейчас исполняется
        AutoLockCS lock(queueCS);
        while(workQueue.front().packet != packet->packetNumber) // это на случай неприхода предыдущего пакета
            workQueue.pop();
        workLine = workQueue.front().line;
        workQueue.pop();
        return true;
    }
    case DeviceCommand_TEXT_MESSAGE:
        log_message("%s", data+1);
        return true;
    default:
        log_warning("%20s", data);
        return false;
    }
}

//============================================================
//обработка пакетов протокола
bool CRemoteDevice::on_packet_received(char *data, int size)
{
    if(size < 4)
        return false;
    unsigned crc = crc32_stm32(0xFFFFFFFF, (unsigned*)data, size-4);
    unsigned receivedCrc = *(unsigned*)(data+size-4);
    if(crc != receivedCrc)
    {
        for(int i=0; i<size; i++)
            log_warning("%c", data[i]);
        missedReceives++;
        return false;
    }

    switch(*data)
    {
    case DeviceCommand_PACKET_RECEIVED:
    {
        AutoLockCS lock(queueCS);
        if(!commandQueue.empty() && commandQueue.front()->packetNumber == ((PacketReceived*)data)->packetNumber) //устройство приняло посланный пакет
        {
            //printf("suc receive number %d\n", ((PacketReceived*)data)->packetNumber);
            delete commandQueue.front();
            commandQueue.pop();
            SetEvent(eventPacketReceived);
            return true;
        }
        else //устройство приняло какой-то непонятный пакет
        {
            log_warning("err receive number %d\n", ((PacketReceived*)data)->packetNumber);
            SetEvent(eventPacketReceived);
            return false;
        }
    }
    case DeviceCommand_PACKET_ERROR_CRC:
        missedHalfSend++;
        SetEvent(eventPacketReceived);
        return true;

    case DeviceCommand_ERROR_PACKET_NUMBER:
    {
        AutoLockCS lock(queueCS);
        log_warning("kosoi nomer %d\n", ((PacketErrorPacketNumber*)data)->packetNumber);
        if(!commandQueue.empty())
            log_warning("nado %d\n", commandQueue.front()->packetNumber);
        return false;
    }

    default:
        return process_packet(data, size - 4);
    }
    return false;
}

//============================================================
DWORD WINAPI CRemoteDevice::send_thread(void *__this)
{
    CRemoteDevice *_this = (CRemoteDevice*)__this;

    while(true)
    {
        WaitForSingleObject(_this->eventQueueAdd, INFINITE);
        while(true)
        {
            EnterCriticalSection(&_this->queueCS);
            if(_this->commandQueue.empty())
            {
                LeaveCriticalSection(&_this->queueCS);
                break;
            }
            else
            {
                auto packet = _this->commandQueue.front();
                _this->comPort->send_data(&packet->size + 1, packet->size - 1);
                LeaveCriticalSection(&_this->queueCS);
                //log_message("send number %d\n", packet->packetNumber);
                //printf("send number %d\n", packet->packetNumber);
            }
            DWORD result = WaitForSingleObject(_this->eventPacketReceived, 100);
            if(result == WAIT_TIMEOUT)
            {
                ++_this->missedSends;
                ResetEvent(_this->eventPacketReceived);
            }
            //Sleep(500);
        }
    }
    return 0;
}

