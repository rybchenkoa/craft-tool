#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <math.h>
#include <float.h>
#include "IRemoteDevice.h"
#include "log.h"
#include "AutoLockCS.h"
#include "config_defines.h"

//====================================================================================================
inline double pow2(double x)
{
    return x*x;
}

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
    lastQueue = -1;

    for(int i = 0; i < NUM_COORDS; ++i)
    {
        scale[i]            = HUGE_VAL;
        lastPosition.r[i]   = HUGE_VAL;
        velocity[i]         = HUGE_VAL;
        acceleration[i]     = HUGE_VAL;
        currentCoords.r[i]  = HUGE_VAL;
        lastDelta.r[i]      = HUGE_VAL;
    }
    minStep = HUGE_VAL;
    secToTick = HUGE_VAL;
    feed = HUGE_VAL;
    moveMode = MoveMode_LINEAR;
    fractSended = false;

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
    PacketQueued element;
    element.line = pushLine;
    element.data = packet;
    commandQueue.push(element);

    SetEvent(eventQueueAdd);
}

//============================================================
template<typename T>
void CRemoteDevice::push_packet_modal(T *packet)
{
    AutoLockCS lock(queueCS);

    packet->size = sizeof(*packet);
    commandQueueMod.push((PacketCommon*)packet);

    SetEvent(eventQueueAdd);
}

//============================================================
void CRemoteDevice::set_move_mode(MoveMode mode)
{
    if(mode == moveMode)
        return;
    moveMode = mode;
    set_fract();

    auto packet = new PacketInterpolationMode;
    packet->command = DeviceCommand_MOVE_MODE; //сменить режим перемещения
    packet->mode = mode; //новый режим
    push_packet_common(packet);
}

//============================================================
//оповещение о конце траектории
void CRemoteDevice::set_fract()
{
    if(fractSended)
        return;

    auto packet = new PacketFract;
    packet->command = DeviceCommand_SET_FRACT;
    push_packet_common(packet);
//log_message("send fract\n");
    fractSended = true;
}

//============================================================
void CRemoteDevice::set_position(Coords pos)
{
    //emit coords_changed(pos.r[0], pos.r[1], pos.r[2]);

    if(lastPosition.r[0] == HUGE_VAL) //если шлём в первый раз
        lastPosition = currentCoords; //то последними посланными считаем текущие реальные

    auto packet = new PacketMove;
    packet->command = DeviceCommand_MOVE; //двигаться в заданную точку

    for(int i = 0; i < NUM_COORDS; ++i)
        packet->coord[i] = int(pos.r[i] * scale[i]);    //переводим мм в шаги

    Coords delta;
    int referenceLen = 0; //максимальное число шагов по координате
    int reference = 0;    //номер самой длинной по шагам координаты

    for(int i = 0; i < NUM_COORDS; ++i)
    {
        delta.r[i] = pos.r[i] - lastPosition.r[i];

        int steps = abs(int(delta.r[i] * scale[i]));
        if(steps > referenceLen)
        {
            referenceLen = steps;
            reference = i;
        }
    }

    lastPosition = pos;

    if(referenceLen == 0)
    {
        delete packet;
        return;
    }

    double length = 0;
    for(int i = 0; i < NUM_COORDS; ++i)
        length += pow2(delta.r[i]);

    length = sqrt(length);

    double lengths[NUM_COORDS];
    for(int i = 0; i < NUM_COORDS; ++i)
        lengths[i] = fabs(delta.r[i]);

    packet->refCoord = reference;
    packet->invProj = float(length / (lengths[reference] * scale[reference]));
    packet->uLength = length * 1000; //в микронах, чтобы хватило разрядов и точности

    if(lastDelta.r[0] != HUGE_VAL)
    {
        double scalar = 0;
        double scalar1 = 0, scalar2 = 0;
        for(int i = 0; i < NUM_COORDS; ++i)
        {
            scalar += lastDelta.r[i] * delta.r[i];
            scalar1 += delta.r[i] * delta.r[i];
            scalar2 += lastDelta.r[i] * lastDelta.r[i];
        }
        double cosA = scalar / sqrt(scalar1 * scalar2);

        if(cosA < 1 - 0.01) //если направление движения сильно изменилось
            set_fract();
    }

    lastDelta = delta;

    //находим максимальное время движения по отдельной координате
    double timeMove = lengths[0] / velocity[0];
    for(int i = 1; i < NUM_COORDS; ++i)
    {
        double time = lengths[i] / velocity[i];
        if(timeMove < time)
            timeMove = time;
    }
    double velValue = length / timeMove; //максимальная скорость движения в заданном направлении

    //ограничиваем скорость подачей
    if(moveMode == MoveMode_LINEAR)
        if(velValue > feed)
            velValue = feed;

    packet->velocity = float(velValue / secToTick);

    //находим максимальное ускорение по опорной координате
    timeMove = lengths[0] / acceleration[0];
    for(int i = 1; i < NUM_COORDS; ++i)
    {
        double time = lengths[i] / acceleration[i];
        if(timeMove < time)
            timeMove = time;
    }
    double accValue = length / timeMove; //максимальное ускорение в заданном направлении
    packet->acceleration = float(accValue / (secToTick * secToTick));

    //log_message("   GO TO %d, %d, %d\n", packet->coord[0], packet->coord[1], packet->coord[2]);
    push_packet_common(packet);

    fractSended = false;
}


//============================================================
void CRemoteDevice::wait(double time)
{
    set_fract();

    auto packet = new PacketWait;
    packet->command = DeviceCommand_WAIT;
    packet->delay = int(time*1000); //задержка
    push_packet_common(packet);
}

//============================================================
void CRemoteDevice::set_bounds(Coords rMin, Coords rMax)
{
    auto packet = new PacketSetBounds;
    packet->command = DeviceCommand_SET_BOUNDS;
    for(int i = 0; i < NUM_COORDS; ++i)
    {
        packet->minCoord[i] = int(rMin.r[i]*scale[i]);
        packet->maxCoord[i] = int(rMax.r[i]*scale[i]);
    }
    push_packet_common(packet);
    //this->rMin = rMin;
    //this->rMax = rMax;
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
        packet->maxVelocity[i] = float(velocity[i]);
        packet->maxAcceleration[i] = float(acceleration[i]);
    }
    push_packet_common(packet);
}

//============================================================
//мм/сек
void CRemoteDevice::set_feed(double feed)
{
    set_fract();

    auto packet = new PacketSetFeed;
    packet->command = DeviceCommand_SET_FEED;
    packet->feed = float16(float(feed));
    push_packet_common(packet);
    this->feed = feed;
}

//============================================================
void CRemoteDevice::set_feed_multiplier(double multiplier)
{
    auto packet = new PacketSetFeedMult;
    packet->command = DeviceCommand_SET_FEED_MULT;
    if(multiplier < 1e-3)
        multiplier = 1e-3; //защита от глюков
    packet->feedMult = float16(float(multiplier));
    push_packet_modal(packet);
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
void CRemoteDevice::pause_moving(bool needStop)
{
    auto packet = new PacketPause;
    packet->command = DeviceCommand_PAUSE;
    packet->needStop = needStop ? 1 : 0;
    push_packet_modal(packet);
}

//============================================================
void CRemoteDevice::reset_packet_queue()
{
    auto packet = new PacketResetPacketNumber;
    packet->command = DeviceCommand_RESET_PACKET_NUMBER;
    push_packet_modal(packet);
}

//============================================================
void CRemoteDevice::init()
{
    packetNumber = -1;
    reset_packet_queue();

    auto try_get_float = [](const char *key) -> float
    {
        float value;
        if(!g_config->get_float(key, value))
            throw(std::string("key not found in konfig: ") + key);
        return value;
    };

    secToTick = 1000000.0;
    double stepsPerMm[NUM_COORDS];
    stepsPerMm[0] = try_get_float(CFG_STEPS_PER_MM "X");
    stepsPerMm[1] = try_get_float(CFG_STEPS_PER_MM "Y");
    stepsPerMm[2] = try_get_float(CFG_STEPS_PER_MM "Z");

    scale[0] = stepsPerMm[0];
    scale[1] = stepsPerMm[1];
    scale[2] = stepsPerMm[2];

    minStep = 1/std::max(scale[0], std::max(scale[1], scale[2]));

    velocity[0] = try_get_float(CFG_MAX_VELOCITY "X") / 60;
    velocity[1] = try_get_float(CFG_MAX_VELOCITY "Y") / 60;
    velocity[2] = try_get_float(CFG_MAX_VELOCITY "Z") / 60;

    acceleration[0] = try_get_float(CFG_MAX_ACCELERATION "X");
    acceleration[1] = try_get_float(CFG_MAX_ACCELERATION "Y");
    acceleration[2] = try_get_float(CFG_MAX_ACCELERATION "Z");

    double feed = 10.0;
    double stepSize[NUM_COORDS] = {1.0/scale[0], 1.0/scale[1], 1.0/scale[2]};

    set_velocity_and_acceleration(velocity, acceleration);
    set_feed(feed);
    set_step_size(stepSize);
    set_fract();
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
const Coords* CRemoteDevice::get_current_coords()
{
    return &currentCoords;
}

double CRemoteDevice::get_min_step()
{
    return minStep;
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
    Q_UNUSED(size)
    switch(*data)
    {
    case DeviceCommand_SERVICE_COORDS:
    {
        PacketServiceCoords *packet = (PacketServiceCoords*) data;
        for(int i = 0; i < NUM_COORDS; ++i)
            currentCoords.r[i] = packet->coords[i] / scale[i];
        emit coords_changed(currentCoords.r[0], currentCoords.r[1], currentCoords.r[2]);
        //log_message("eto ono (%d, %d, %d)\n", packet->coords[0], packet->coords[1], packet->coords[2]);
        return true;
    }
    case DeviceCommand_SERVICE_COMMAND:
    {
        PacketServiceCommand *packet = (PacketServiceCommand*) data; //находим строку, которая сейчас исполняется
        AutoLockCS lock(queueCS);
        while(!workQueue.empty() && workQueue.front().packet != packet->packetNumber) // это на случай неприхода предыдущего пакета
            workQueue.pop_front();
        if(!workQueue.empty())
        {
            workLine = workQueue.front().line;
            workQueue.pop_front();
        }
        else
            log_warning("current line queue is empty\n", data);
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
        //log_message("packet received %d\n", ((PacketReceived*)data)->packetNumber);
        AutoLockCS lock(queueCS);
        if(lastQueue == 0 &&
           !commandQueue.empty() &&
           commandQueue.front().data->packetNumber == ((PacketReceived*)data)->packetNumber) //устройство приняло посланный пакет
        {
            //log_message("suc receive number %d\n", ((PacketReceived*)data)->packetNumber);
            delete commandQueue.front().data;
            commandQueue.pop();
            lastQueue = -1;
            ++packetNumber;
            SetEvent(eventPacketReceived);
            return true;
        }
        else if(lastQueue == 1 &&
                !commandQueueMod.empty() &&
                commandQueueMod.front()->packetNumber == ((PacketReceived*)data)->packetNumber)
        {
            //log_message("suc receive number2 %d\n", ((PacketReceived*)data)->packetNumber);
            delete commandQueueMod.front();
            commandQueueMod.pop();
            lastQueue = -1;
            ++packetNumber;
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
    case DeviceCommand_PACKET_REPEAT:
    {
        //log_warning("packet repeat %d\n", ((PacketReceived*)data)->packetNumber);
        AutoLockCS lock(queueCS);
        if(lastQueue == 0 &&
           !commandQueue.empty() &&
           commandQueue.front().data->packetNumber == ((PacketReceived*)data)->packetNumber) //устройство приняло посланный пакет
        {
            //log_warning("suc repeat number %d\n", ((PacketReceived*)data)->packetNumber);
            delete commandQueue.front().data;
            commandQueue.pop();
            lastQueue = -1;
            ++packetNumber;
            SetEvent(eventPacketReceived);
            return true;
        }
        else if(lastQueue == 1 &&
                !commandQueueMod.empty() &&
                commandQueueMod.front()->packetNumber == ((PacketReceived*)data)->packetNumber)
        {
            //log_warning("suc repeat number2 %d\n", ((PacketReceived*)data)->packetNumber);
            delete commandQueueMod.front();
            commandQueueMod.pop();
            lastQueue = -1;
            ++packetNumber;
            SetEvent(eventPacketReceived);
            return true;
        }
        else //устройство приняло какой-то непонятный пакет
        {
            //log_warning("err repeat number %d\n", ((PacketReceived*)data)->packetNumber);
            SetEvent(eventPacketReceived);
            return false;
        }
    }
    case DeviceCommand_QUEUE_FULL:
    {
        AutoLockCS lock(queueCS);
        if(lastQueue == 0)
            workQueue.pop_back();
        lastQueue = -1;
    }
    case DeviceCommand_PACKET_ERROR_CRC:
        missedHalfSend++;
        SetEvent(eventPacketReceived);
        return true;

    case DeviceCommand_ERROR_PACKET_NUMBER:
    {
        AutoLockCS lock(queueCS);
        log_warning("kosoi nomer %d\n", ((PacketErrorPacketNumber*)data)->packetNumber);
        log_warning("nado %d\n", packetNumber);
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
            int queue = -1;
            if(_this->lastQueue == 0 && !_this->commandQueue.empty())
                queue = 0;
            else if(_this->lastQueue == 1 && !_this->commandQueueMod.empty())
                queue = 1;
            else if(!_this->commandQueueMod.empty())
                    queue = 1;
            else if(!_this->commandQueue.empty())
                    queue = 0;

            if(queue == -1)
            {
                LeaveCriticalSection(&_this->queueCS);
                break;
            }
            else
            {
                PacketCommon *packet;
                if(queue == 0)
                    packet = _this->commandQueue.front().data;
                else if(queue == 1)
                    packet = _this->commandQueueMod.front();

                packet->packetNumber = _this->packetNumber;
                _this->make_crc((char*)packet);

                if(queue == 0)
                {
                    WorkPacket work;
                    work.line = _this->commandQueue.front().line;
                    work.packet = packet->packetNumber;
                    _this->workQueue.push_back(work);
                }
                _this->lastQueue = queue;
                _this->comPort->send_data(&packet->size + 1, packet->size - 1);
                //log_message("send%d number %d\n", queue, packet->packetNumber);
                LeaveCriticalSection(&_this->queueCS);

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

