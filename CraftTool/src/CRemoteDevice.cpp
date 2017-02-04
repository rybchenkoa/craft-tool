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
	inited = false;

    for(int i = 0; i < MAX_AXES; ++i)
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

    InitializeCriticalSectionAndSpinCount(&queueCS, 1000);
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

	if (mode == MoveMode_LINEAR) //если едем на G1, то не надо замедляться перед G0
		set_fract(); //а при переходе с быстрых перемещений на силовые скорость надо сбросить заранее

	if (mode == MoveMode_FAST)
		mode = MoveMode_LINEAR;

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
void CRemoteDevice::set_position(Coords posIn)
{
    //emit coords_changed(pos.r[0], pos.r[1], pos.r[2]);

    if(lastPosition.r[0] == HUGE_VAL) //если шлём в первый раз
        lastPosition = currentCoords; //то последними посланными считаем текущие реальные

    auto packet = new PacketMove;
    packet->command = DeviceCommand_MOVE; //двигаться в заданную точку

	Coords pos = posIn;
	for (int i = 0; i < MAX_AXES; ++i) //заполняем подчиненные оси
		if (slaveAxes[i] >= 0)
			pos.r[slaveAxes[i]] = pos.r[i];

	//сначала задаем используемые координаты
    for (int i = 0; i < MAX_AXES; ++i)
	{
		int index = toDeviceIndex[i];
		if (usedAxes[i])
			packet->coord[index] = (int)floor(pos.r[i] * scale[i] + 0.5);    //переводим мм в шаги
		else
			packet->coord[index] = 0;

		if (invertAxe[i])
			packet->coord[index] *= -1;
	}

    Coords delta;
    int referenceLen = 0; //максимальное число шагов по координате
    int reference = 0;    //номер самой длинной по шагам координаты

    for(int i = 0; i < MAX_AXES; ++i)
    {
		if (!usedAxes[i])
			continue;

		if (moveMode != MoveMode_HOME)
			delta.r[i] = pos.r[i] - lastPosition.r[i];
		else
			delta.r[i] = pos.r[i];

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
    for(int i = 0; i < NUM_COORDS; ++i) //TODO для поворотной оси неизвестно как считать
        length += pow2(delta.r[i]);

    length = sqrt(length);

    double lengths[NUM_COORDS]; //у подчиненных осей те же параметры, поэтому смотрим только на главные
    for(int i = 0; i < NUM_COORDS; ++i)
        lengths[i] = fabs(delta.r[i]);

    packet->refCoord = toDeviceIndex[reference];
    packet->length = float(length);
    packet->uLength = length * 1000; //в микронах, чтобы хватило разрядов и точности

    if(lastDelta.r[0] != HUGE_VAL)
    {
        double scalar = 0;
        double scalar1 = 0, scalar2 = 0;
        for(int i = 0; i < NUM_COORDS; ++i) //поворотные оси тоже учитываем, так как на них тоже действует физика
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
//задает номера концевиков всех осей
void CRemoteDevice::set_switches(SwitchGroup group, int pins[MAX_AXES])
{
	auto packet = new PacketSetSwitches;
	packet->command = DeviceCommand_SET_SWITCHES;
	packet->group = group;
	for(int i = 0; i < MAX_AXES; ++i)
		packet->pins[toDeviceIndex[i]] = pins[i];
	packet->polarity = switchPolarity;
	push_packet_common(packet);
}

//============================================================
//записывает аппаратные координаты
void CRemoteDevice::set_coord(Coords posIn, bool used[MAX_AXES])
{
	auto packet = new PacketSetCoords;
	packet->command = DeviceCommand_SET_COORDS;

	Coords pos = posIn;
	for (int i = 0; i < MAX_AXES; ++i) //заполняем подчиненные оси
		if (slaveAxes[i] >= 0)
			pos.r[slaveAxes[i]] = pos.r[i];

	int usedBit = 0;
	//сначала задаем используемые координаты
    for (int i = 0; i < MAX_AXES; ++i)
	{
		int index = toDeviceIndex[i];
		if (usedAxes[i])
		{
			packet->coord[index] = (int)floor(pos.r[i] * scale[i] + 0.5);    //переводим мм в шаги
			lastPosition.r[i] = pos.r[i];
		}
		else
			packet->coord[index] = 0;

		if (invertAxe[i])
			packet->coord[index] *= -1;

		if (used[i])
			usedBit |= 1 << index;
	}

	packet->used = usedBit;
	push_packet_common(packet);
}

//============================================================
//мм/сек, мм/сек^2
void CRemoteDevice::set_velocity_and_acceleration(double velocity[MAX_AXES], double acceleration[MAX_AXES])
{
    auto packet = new PacketSetVelAcc;
    packet->command = DeviceCommand_SET_VEL_ACC; //задать макс скорость и ускорение
    for(int i = 0; i < MAX_AXES; ++i)
    {
        packet->maxVelocity[toDeviceIndex[i]] = float(velocity[i]);
        packet->maxAcceleration[toDeviceIndex[i]] = float(acceleration[i]);
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
void CRemoteDevice::set_step_size(double stepSize[MAX_AXES])
{
    auto packet = new PacketSetStepSize;
    packet->command = DeviceCommand_SET_STEP_SIZE; //задать макс скорость и ускорение
    for(int i = 0; i < MAX_AXES; ++i)
    {
        packet->stepSize[toDeviceIndex[i]] = float(stepSize[i]);
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
std::vector<std::string> split(std::string str, char delim)
{
	std::vector<std::string> result;
	std::istringstream ss(str);
	std::string s;
	while (std::getline(ss, s, delim))
		result.push_back(s);
	return result;
}

//============================================================
int letter_to_axe(char letter)
{
	switch (letter)
	{
		case 'x': case 'X': return 0;
		case 'y': case 'Y': return 1;
		case 'z': case 'Z': return 2;
		case 'a': case 'A': return 3;
		case 'b': case 'B': return 4;
		default:  return -1;
	}
};

//============================================================
bool read_double(const std::string &str, int &pos, double &value)
{
	std::string val;
	while (pos < str.size() && (str[pos] >= '0' && str[pos] <= '9' || str[pos] == ',' || str[pos] == '.'))
		val.push_back(str[pos++]);
	try
	{
		value = std::stod(val);
		return true;
	}
	catch(...)
	{
		return false;
	}
}

//============================================================
void CRemoteDevice::homing()
{
    //строка формата [[координата сдвиг ...], ...]
	std::vector<std::string> moves = split(homingScript, ',');
	double feed = 600; //по умолчанию сантиметр в секунду
	auto lastMode = moveMode;
	set_move_mode(MoveMode_HOME);
	for (auto str = moves.begin(); str != moves.end(); ++str)
	{
		Coords pos; //по умолчанию везде смещения нулевые
		for (int i = 0; i < str->length(); )
		{
			char symbol = (*str)[i++];
			if (symbol == ' ')
				continue;

			int coordNum = letter_to_axe(symbol);
			if (coordNum != -1)
				if (!read_double(*str, i, pos.r[coordNum])) //координаты задаются инкрементально
				{
					log_warning("invalid homing string");
					return;
				}

			if (symbol == 'f' || symbol == 'F')
				if (!read_double(*str, i, feed) || feed < 0) //мм/мин
				{
					log_warning("invalid homing string");
					return;
				}
		}

		set_position(pos);
	}
	set_move_mode(lastMode);
	set_coord(coordHome, usedAxes);
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
            throw(std::string("key not found in config: ") + key);
        return value;
    };

	auto try_get_string = [](const char *key) -> std::string
    {
        std::string value;
        if(!g_config->get_string(key, value))
            throw(std::string("key not found in config: ") + key);
        return value;
    };

	std::string coordList = try_get_string(CFG_USED_COORDS); //читаем используемые интерпретатором координаты

	for (int i = 0; i < MAX_AXES; ++i)
		usedCoords[i] = false;

	for (int i = 0; i < coordList.length(); ++i)
	{
		int numAxe = letter_to_axe(coordList[i]);
		if (numAxe == -1)
			throw(std::string("invalid value of 'usedCoords' in config: letter ") + coordList[i] + "'");
		usedCoords[numAxe] = true;
	}
	memcpy(usedAxes, usedCoords, sizeof(usedCoords));

	//читаем подчиненные оси
	std::string slave = CFG_SLAVE;
	std::string AXE_LIST = "XYZAB";
	for (int i = 0; i < AXE_LIST.size(); ++i)
	{
		std::string value, key = slave + AXE_LIST[i];
		if (g_config->get_string(key.c_str(), value))
		{
			int numAxe = letter_to_axe(value[0]);
			if (value.length() != 1 || numAxe == -1 || usedAxes[numAxe] || !usedCoords[i])
				throw("invalid value of '" + value + "' in config: '" + value + "'");
			
			usedAxes[numAxe] = true; //пишем используемые станком оси
			slaveAxes[i] = numAxe;
		}
		else
			slaveAxes[i] = -1;
	}

	//читаем параметры дискретизации
    secToTick = 24000000.0;
	double stepSize[MAX_AXES];
	minStep = 1000000; //километровых шагов уж точно ни у кого не будет
	for (int i = 0; i < MAX_AXES; ++i)
		if (usedAxes[i]) //дискретизация нужна для всех осей
		{
			scale[i] = try_get_float((std::string(CFG_STEPS_PER_MM) + AXE_LIST[i]).c_str());
			stepSize[i] = 1 / scale[i];
			minStep = std::min(minStep, stepSize[i]);
		}
		else
		{
			scale[i] = 0;
			stepSize[i] = 0;
		}

	//читаем скорости и ускорения
	for (int i = 0; i < MAX_AXES; ++i)
		if (usedCoords[i]) //ограничение скорости надо только для ведущих осей
		{
			velocity[i] = try_get_float((std::string(CFG_MAX_VELOCITY) + AXE_LIST[i]).c_str()) / 60;
			acceleration[i] = try_get_float((std::string(CFG_MAX_ACCELERATION) + AXE_LIST[i]).c_str());
		}
		else
		{
			velocity[i] = 0;
			acceleration[i] = 0;
		}

	for (int i = 0; i < MAX_AXES; ++i)
		if (slaveAxes[i] >= 0) //для ведомых осей задаем скорость на основе числа шагов
		{
			int slave = slaveAxes[i];
			double reScale = scale[slave] / scale[i];
			velocity[slave] = velocity[i] * reScale;
			acceleration[slave] = acceleration[i] * reScale;
		}

    double feed = 10.0;

	//назначаем группы выводов, которые будут генерировать сигнал
	std::string pinsMap = try_get_string(CFG_AXE_MAP);
	std::stringstream ss(pinsMap);
	std::string token;
	while (ss >> token)
	{
		bool processed = false;
		if (token.size() == 2)
		{
			int numAxe = letter_to_axe(token[0]);
			int numPin = token[1] - '0';
			if (numAxe >= 0 && numPin >= 0 && numPin < MAX_AXES && 
				std::find(toDeviceIndex.begin(), toDeviceIndex.end(), numAxe) == toDeviceIndex.end())
			{
				processed = true;
				toDeviceIndex.push_back(numAxe);
			}
		}
		if (!processed)
			throw("invalid value of '" + std::string(CFG_AXE_MAP) + "' in config: '" + token + "'");
	}

	for (int i = 0; i < MAX_AXES; ++i)
		if (std::find(toDeviceIndex.begin(), toDeviceIndex.end(), i) == toDeviceIndex.end())
			toDeviceIndex.push_back(i);

	// строим обратный индекс
	fromDeviceIndex.resize(MAX_AXES);
	for (int i = 0; i < MAX_AXES; ++i)
		fromDeviceIndex[toDeviceIndex[i]] = i;

	//какие оси надо инвертировать
	std::string inverted;
	if (g_config->get_string(CFG_INVERTED_AXES, inverted))
	{
		for (int i = 0; i < AXE_LIST.size(); ++i)
			if (inverted.find(AXE_LIST[i]) == std::string::npos)
				invertAxe[i] = false;
			else
				invertAxe[i] = true;
	}
	else
		for (int i = 0; i < AXE_LIST.size(); ++i)
			invertAxe[i] = false;

	//читаем концевики
	auto get_in_pin = [](std::string key) -> int
	{
		int value;
		if (g_config->get_int(key.c_str(), value))
		{
			if (value < 0 || value >= MAX_IN_PINS)
				throw ("invalid value of '" + key + "' in config");
		}
		else
			value = -1;
		return value;
	};

	for (int i = 0; i < MAX_AXES; ++i)
	{
		int value = get_in_pin(std::string(CFG_SWITCH_MIN) + AXE_LIST[i]);
		(invertAxe[i] ? switchMax[i] : switchMin[i]) = value;

		value = get_in_pin(std::string(CFG_SWITCH_MAX) + AXE_LIST[i]);
		(invertAxe[i] ? switchMin[i] : switchMax[i]) = value;

		value = get_in_pin(std::string(CFG_SWITCH_HOME) + AXE_LIST[i]);
		switchHome[i] = value;

		std::string key = std::string(CFG_BACK_HOME) + AXE_LIST[i];
		float dValue;
		if (!g_config->get_float(key.c_str(), dValue))
			dValue = 0;
		backHome[i] = dValue;

		key = std::string(CFG_COORD_HOME) + AXE_LIST[i];
		if (!g_config->get_float(key.c_str(), dValue))
			dValue = 0;
		coordHome.r[i] = dValue;
	}

	switchPolarity = 0;
	std::string value;
	if (g_config->get_string(CFG_SWITCH_POLARITY, value))
	{
		for (int i = 0; i < value.size(); ++i)
			if (i > MAX_IN_PINS || value[i] != '0' && value[i] != '1')
				throw ("invalid value of '"  CFG_SWITCH_POLARITY  "' in config");
			else
				if (value[i] == '1')
					switchPolarity |= (1 << i);
	}
	
	g_config->get_string(CFG_HOMING, homingScript);

	set_switches(SwitchGroup_MIN, switchMin);
	set_switches(SwitchGroup_MAX, switchMax);
	set_switches(SwitchGroup_HOME, switchHome);

    set_velocity_and_acceleration(velocity, acceleration);
    set_feed(feed);
    set_step_size(stepSize);
    set_fract();

	inited = true;
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
		if (!inited) return true;
        PacketServiceCoords *packet = (PacketServiceCoords*) data;
        for(int i = 0; i < MAX_AXES; ++i)
            currentCoords.r[i] = packet->coords[fromDeviceIndex[i]] / scale[i] * (invertAxe[i] ? -1 : 1);
        emit coords_changed(currentCoords.r[0], currentCoords.r[1], currentCoords.r[2]); //TODO: переделать на правильные координаты?
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
				//log_message("send timeout\n");
                ResetEvent(_this->eventPacketReceived);
            }
            //Sleep(500);
        }
    }
    return 0;
}

