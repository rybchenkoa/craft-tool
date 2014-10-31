#include "IRemoteDevice.h"

#include "WinSock.h"
//#pragma comment(lib, "wsock32.lib")

CRemoteDevice::CRemoteDevice()
{
	init_crc();
	packetNumber = -1;
	canSend = true;
	missedSends = 0;
	missedReceives = 0;
	missedHalfSend = 0;

	DWORD   threadId;
    HANDLE hThread2 = CreateThread(NULL, 0, send_thread, this, 0, &threadId);
}


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
	if(len)
	{
		switch(len)
		{
			case 1: v = 0xFF000000 & htonl(*buf++); break;
			case 2: v = 0xFFFF0000 & htonl(*buf++); break;
			case 3: v = 0xFFFFFF00 & htonl(*buf++); break;
		}
		crc = ( crc << 8 ) ^ crc32Table[0xFF & ( (crc >> 24) ^ (v ) )];
		crc = ( crc << 8 ) ^ crc32Table[0xFF & ( (crc >> 24) ^ (v >> 8) )];
		crc = ( crc << 8 ) ^ crc32Table[0xFF & ( (crc >> 24) ^ (v >> 16) )];
		crc = ( crc << 8 ) ^ crc32Table[0xFF & ( (crc >> 24) ^ (v >> 24) )];
	}
	return crc;
}


template<typename T>
void CRemoteDevice::push_packet_common(T *packet)
{
	packet->size = sizeof(*packet);
	packet->packetNumber = packetNumber++;
	make_crc((char*)packet);
	commandQueue.push((PacketCommon*)packet);
}


void CRemoteDevice::set_move_mode(MoveMode mode)
{
	auto packet = new PacketInterpolationMode;
	packet->command = DeviceCommand_MOVE_MODE; //сменить режим перемещения
	packet->mode = mode; //новый режим
	push_packet_common(packet);
}


void CRemoteDevice::set_plane(MovePlane plane)
{
	auto packet = new PacketSetPlane;
	packet->command = DeviceCommand_SET_PLANE; //сменить плоскость
	packet->plane = plane; //новая плоскость
	push_packet_common(packet);
}


void CRemoteDevice::set_position(double x, double y, double z)
{
	auto packet = new PacketMove;
	packet->command = DeviceCommand_MOVE; //двигаться
	packet->coord[0] = int(x*scale[0]);
	packet->coord[1] = int(y*scale[1]);
	packet->coord[2] = int(z*scale[2]);
	printf("   GO TO %d, %d, %d\n", packet->coord[0], packet->coord[1], packet->coord[2]);
	push_packet_common(packet);
}


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


void CRemoteDevice::wait(double time)
{
	auto packet = new PacketWait;
	packet->command = DeviceCommand_WAIT;
	packet->delay = int(time*1000); //задержка
	push_packet_common(packet);
}


void CRemoteDevice::set_bounds(double rMin[3], double rMax[3])
{
	auto packet = new PacketSetBounds;
	packet->command = DeviceCommand_SET_BOUNDS;
	for(int i = 0; i < 3; ++i)
	{
		packet->minCoord[i] = int(rMin[i]*scale[i]);
		packet->maxCoord[i] = int(rMax[i]*scale[i]);
	}
	push_packet_common(packet);
}


//мм/сек, мм/сек^2
//переводим в шаг/мкс, шаг/мкс^2
//значения хранятся в виде 1.0/value, поскольку в int
void CRemoteDevice::set_velocity_and_acceleration(double velocity[3], double acceleration[3])
{
	auto packet = new PacketSetVelAcc;
	packet->command = DeviceCommand_SET_VEL_ACC; //задать макс скорость и ускорение
	for(int i = 0; i < 3; ++i)
	{
		packet->maxrVelocity[i] = int(1.0/(velocity[i]*scale[i]/secToTick));
		packet->maxrAcceleration[i] = int(1.0/(acceleration[i]*scale[i]/(secToTick*secToTick)));
	}
	push_packet_common(packet);
}

//мм/сек
void CRemoteDevice::set_feed(double feed)
{
	auto packet = new PacketSetFeed;
	packet->command = DeviceCommand_SET_FEED;
	packet->rFeed = int(1.0/(feed/secToTick));
	push_packet_common(packet);
}


void CRemoteDevice::set_step_size(double stepSize[3])
{
	auto packet = new PacketSetStepSize;
	packet->command = DeviceCommand_SET_STEP_SIZE; //задать макс скорость и ускорение
	for(int i = 0; i < 3; ++i)
	{
		packet->stepSize[i] = float(stepSize[i]);
	}
	push_packet_common(packet);
}


void CRemoteDevice::set_voltage(double voltage[3])
{
	auto packet = new PacketSetVoltage;
	packet->command = DeviceCommand_SET_VOLTAGE;
	for(int i = 0; i < 3; ++i)
	{
		packet->voltage[i] = int(voltage[i]*255);
	}
	push_packet_common(packet);
}


void CRemoteDevice::reset_packet_queue()
{
	auto packet = new PacketResetPacketNumber;
	packet->command = DeviceCommand_RESET_PACKET_NUMBER;
	push_packet_common(packet);
}


void CRemoteDevice::init()
{
	packetNumber = -1;
	reset_packet_queue();

	secToTick = 1000000.0;
	subSteps = 4;
	double stepsPerMm = 48.0;
	scale[0] = stepsPerMm*(1<<subSteps);
	scale[1] = stepsPerMm*(1<<subSteps);
	scale[2] = stepsPerMm*(1<<subSteps);
	double maxVelocity[3] = {100.0, 100.0, 100.0};
	double maxAcceleration[3] = {100.0, 100.0, 100.0};
	double feed = 100.0;
	double stepSize[3] = {1.0/scale[0], 1.0/scale[1], 1.0/scale[2]};
	double voltage[3] = {0.7, 0.7, 0.7};

	set_velocity_and_acceleration(maxVelocity, maxAcceleration);
	set_feed(feed);
	set_step_size(stepSize);
	set_voltage(voltage);
}


void CRemoteDevice::make_crc(char *packet)
{
	int size = *packet-1;
	char *buffer = packet+1;
	int &crc = *(int*)(buffer+size-4);

	crc = crc32_stm32(0xFFFFFFFF, (unsigned int*)(buffer), size-4);
}


bool CRemoteDevice::need_next_command()
{
	return (commandQueue.size() < 100);
}

bool CRemoteDevice::queue_empty()
{
	return commandQueue.empty();
}

bool CRemoteDevice::on_packet_received(char *data, int size)
{
	if(size < 4)
		return false;
	unsigned crc = crc32_stm32(0xFFFFFFFF, (unsigned*)data, size-4);
	unsigned receivedCrc = *(unsigned*)(data+size-4);
	if(crc != receivedCrc)
	{
		for(int i=0; i<size; i++)
			printf("%c", data[i]);
		missedReceives++;
		//canSend = true;
		return false;
	}

	switch(*data)
	{
	case DeviceCommand_PACKET_RECEIVED:
		if(commandQueue.front()->packetNumber == ((PacketReceived*)data)->packetNumber)
		{
			//printf("suc receive number %d\n", ((PacketReceived*)data)->packetNumber);
			commandQueue.pop();
			canSend = true;
			return true;
		}
		else
		{
			printf("err receive number %d\n", ((PacketReceived*)data)->packetNumber);
			canSend = true;
			return false;
		}

	case DeviceCommand_PACKET_ERROR_CRC:
		missedHalfSend++;
		canSend = true;
		return true;

	case DeviceCommand_ERROR_PACKET_NUMBER:
		printf("kosoi nomer %d\n", ((PacketErrorPacketNumber*)data)->packetNumber);
		return false;
	default:
		printf("%s\n", data);
	}
	return false;
}


DWORD WINAPI CRemoteDevice::send_thread(void *__this)
{
	CRemoteDevice *_this = (CRemoteDevice*)__this;

	while(true)
	{
		if(!_this->commandQueue.empty())
		{
			auto packet = _this->commandQueue.front();
			_this->comPort->send_data(&packet->size + 1, packet->size - 1);
			//printf("send number %d\n", packet->packetNumber);
			_this->canSend = false;
		}
		else
		{
			Sleep(10);
			continue;
		}

		int counter = 10;
		while(!_this->canSend && counter-- != 0) //ждём ответа о том, что дошёл
			Sleep(300);
		//Sleep(4000);
		if(!_this->canSend)
			_this->missedSends++; //если не дошёл, регистрируем
		/*else
		{
			delete _this->commandQueue.front();
			_this->commandQueue.pop();
		}*/

		_this->canSend = true; //шлём следующий или тот же
	}

    return 0;
}

