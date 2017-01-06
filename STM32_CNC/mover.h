#include "fifo.h"
#include "packets.h"
#include "motor.h"
#include "sys_timer.h"
#include "led.h"
#include "float16.h"

void send_packet(char *packet, int size);
void process_packet(char *packet, int size);

struct Track
{
	int segments;  //число отрезков в траектории
	int uLength;  //общая длина пути в микронах
};

class Receiver
{
public:
	FIFOBuffer<MaxPacket, 5> queue; //28*32+8 = 904
	FIFOBuffer<Track, 6> tracks;
	PacketCount packetNumber;

	void init()
	{
		queue.Clear();
		tracks.Clear();
		packetNumber = char(-1);
	}
};

Receiver receiver;


//=========================================================================================
void send_wrong_crc()
{
	PacketErrorCrc packet;
	packet.command = DeviceCommand_PACKET_ERROR_CRC;
	packet.crc = calc_crc((char*)&packet, sizeof(packet) - 4);
	send_packet((char*)&packet, sizeof(packet));
}


//=========================================================================================
void send_packet_received(int number)
{
	PacketReceived packet;
	packet.command = DeviceCommand_PACKET_RECEIVED;
	packet.packetNumber = number;
	packet.crc = calc_crc((char*)&packet, sizeof(packet) - 4);
	send_packet((char*)&packet, sizeof(packet));
}


//=========================================================================================
void send_packet_repeat(int number)
{
	PacketReceived packet;
	packet.command = DeviceCommand_PACKET_REPEAT;
	packet.packetNumber = number;
	packet.crc = calc_crc((char*)&packet, sizeof(packet) - 4);
	send_packet((char*)&packet, sizeof(packet));
}


//=========================================================================================
void send_packet_queue_full(int number)
{
	PacketReceived packet;
	packet.command = DeviceCommand_QUEUE_FULL;
	packet.packetNumber = number;
	packet.crc = calc_crc((char*)&packet, sizeof(packet) - 4);
	send_packet((char*)&packet, sizeof(packet));
}


//=========================================================================================
void send_packet_error_number(int number)
{
	PacketErrorPacketNumber packet;
	packet.command = DeviceCommand_ERROR_PACKET_NUMBER;
	packet.packetNumber = number;
	packet.crc = calc_crc((char*)&packet, sizeof(packet) - 4);
	send_packet((char*)&packet, sizeof(packet));
}


//=========================================================================================
void push_received_packet(char * __restrict packet, int size)
{
	int size4 = (size+3)/4; //копируем сразу по 4 байта
	int *src = (int*)packet;
	int *dst = (int*)&receiver.queue.End();
	for(int i=0; i<size4; ++i)
		dst[i] = src[i];
	receiver.queue.Push();
}


//=========================================================================================
void send_packet_service_coords(int coords[NUM_COORDS])
{
	PacketServiceCoords packet;
	packet.command = DeviceCommand_SERVICE_COORDS;
	for(int i = 0; i < NUM_COORDS; ++i)
		packet.coords[i] = coords[i];
	packet.crc = calc_crc((char*)&packet, sizeof(packet) - 4);
	send_packet((char*)&packet, sizeof(packet));
}


//=========================================================================================
void send_packet_service_command(PacketCount number)
{
	PacketServiceCommand packet;
	packet.command = DeviceCommand_SERVICE_COMMAND;
	packet.packetNumber = number;
	packet.crc = calc_crc((char*)&packet, sizeof(packet) - 4);
	send_packet((char*)&packet, sizeof(packet));
}


//=========================================================================================
void on_packet_received(char * __restrict packet, int size)
{
	//led.show();
	int crc = calc_crc(packet, size-4);
	int receivedCrc = *(int*)(packet+size-4);
	if(crc != receivedCrc)
	{
		//log_console("\n0x%X, 0x%X\n", receivedCrc, crc);
		send_wrong_crc();
		return;
	}
	if(size > sizeof(MaxPacket))
		log_console("ERR: rec pack %d, max %d\n", size, sizeof(MaxPacket));
	if(*packet == DeviceCommand_RESET_PACKET_NUMBER)
	{
		receiver.packetNumber = PacketCount(-1);
		send_packet_received(-1);
	}
	else
	{
		PacketCommon* common = (PacketCommon*)packet;
		if (common->packetNumber == receiver.packetNumber) //до хоста не дошёл ответ о принятом пакете
		{
			send_packet_repeat(receiver.packetNumber);                       //шлём ещё раз
		}
		else if(common->packetNumber == PacketCount(receiver.packetNumber + 1)) //принят следующий пакет
		{
			process_packet(packet, size);
		}
		else
		{
			send_packet_error_number(receiver.packetNumber);
			//log_console("\n%d %d\n", common->packetNumber, receiver.packetNumber);
		}
	}
}


//=========================================================================================
//примерное вычисление корня
int isqrt(int value)
{
	if(value <= 0) return 0;

	int leftBits = __clz(value); //бит слева до первого значащего бита, 0 = сразу попался
	unsigned int minVal = 1 << ((31-leftBits)/2); //примерный корень
	/*unsigned int maxVal = minVal*2; //это его правая граница
	//методом половинного деления добиваем до точного значения, макс. 16 итераций
	while(minVal != maxVal)
	{
		int current = (minVal + maxVal)/2;
		int value2 = current * (current+1); //это не магия, ~= (a+0.5)*(a+0.5)
		if (value2 >= value)
			maxVal = current;
		else
			minVal = current+1;
	}
	return minVal;*/
	//dx = dy/(2*x0) - dy^2/(8*x0^3)
	minVal = (value/minVal + minVal)/2;
	minVal = (value/minVal + minVal)/2;
	minVal = (value/minVal + minVal)/2;
	return minVal;	
}


//=====================================================================================================
int iabs(int value)
{
	return value>0 ? value : -value;
}


//=====================================================================================================
int ipow2(int value)
{
	return value*value;
}


//=====================================================================================================
int isign(int value)
{
	if(value > 0) return 1;
	else if (value < 0) return -1;
	else return 0;
}


//=========================================================================================
class Mover
{
public:
	float16 maxVelocity[NUM_COORDS];      // мм/мкс
	float16 maxAcceleration[NUM_COORDS];  // мм/мкс^2
	float16 stepLength[NUM_COORDS]; // мм/шаг

	int coord[NUM_COORDS];    //текущие координаты
	int stopTime;             //время следующей остановки

	int from[NUM_COORDS];     //откуда двигаемся
	int to[NUM_COORDS];       //куда двигаемся

	MoveMode interpolation;
	bool needStop;            //принудительная остановка
	float16 feedMult;         //заданная из интерфейса скорость движения

bool canLog;

	//расчёт ускорения не совсем корректен
	//при движении по кривой максимальное ускорение меняется из-за изменения проекции на оси
	//и расстояние торможения неизвестно
	//но на малых скоростях это не должно проявляться
	//поскольку расстояние торможения маленькое
	//и касательная к кривой поворачивается не сильно

	//=====================================================================================================
	enum State
	{
		FAST = 0,
		LINEAR,
	};


	//=====================================================================================================
	enum OperateResult
	{
		END = 1,
		WAIT,
	};


	//=====================================================================================================
	/*
	диапазоны
	скорость:
	1 шаг/10 сек
	10 000 000 шаг/сек   (если 0.0001 мм/шаг)
	240 000 000 тактов/шаг
	2.4 такта/шаг

	ускорение:
	100 шаг/сек2      (если 0.1 мм на шаг)
	100 000 000 шаг/сек2    (если 0.0001 мм/шаг)
	5 760 000 000 000 тактов2/шаг
	5 760 000 тактов2/шаг

	v += a*dt
	dt - 1000, 10000 тактов


	для float
	сложение 147 тактов
	вычитание 153
	умножение 209
	деление 597 (на M0 ядре)
	сравнение 137
	*/

	struct LinearData
	{
		bool enabled;          //моторы включены
		int refCoord;          //индекс координаты, по которой шагаем
		int size[NUM_COORDS];  //размеры линии
		int err[NUM_COORDS];   //ошибка координат
		int sign[NUM_COORDS];  //изменение координаты при превышении ошибки {-1 || 1}
		int last[NUM_COORDS];   //последние координаты: для которых была посчитана ошибка
		float16 velCoef[NUM_COORDS]; //на что умножить скорость, чтобы получить число тактов на шаг

		float16 maxFeedVelocity;	//скорость подачи, мм/тик
		float16 acceleration;		//ускорение, мм/тик^2
		float16 velocity;			//скорость на прошлом шаге
		float16 invProj;			//(длина опорной координаты / полную длину)
		int state;			//ускорение/замедление/стабильное движение
		int lastTime;		//предыдущее время, нужно для вычисления разницы
		float16 lastVelocity; //предыдущая скорость
	};
	LinearData linearData;



	//=====================================================================================================
	void start_motors()
	{
		if (!linearData.enabled)
		{
			for (int i = 0; i < NUM_COORDS; ++i)
				motor[i].start(MAX_STEP_TIME);
			linearData.enabled = true;
		}
	}

	//=====================================================================================================
	void stop_motors()
	{
		if (linearData.enabled)
		{
			for (int i = 0; i < NUM_COORDS; ++i)
				motor[i].stop();
			linearData.enabled = false;
		}
	}

	//=====================================================================================================
	void compute_error()
	{
		int ref = linearData.refCoord;
		//допустим, размеры по x, y = a, b  , отрезок начинается с 0
		//тогда уравнение будет a*y = b*x;   a*dy = b*dx;  a*dy - b*dx = 0
		//ошибка по y относительно x при смещении:  (a*dy - b*dx) / a
		int dx = (motor[ref]._position - linearData.last[ref]) * linearData.sign[ref]; //находим изменение опорной координаты
		for (int i = 0; i < NUM_COORDS; ++i)
		{
			int dy = (motor[i]._position - linearData.last[i]) * linearData.sign[i]; //находим изменение текущей координаты
			int derr = dy * linearData.size[ref] - dx * linearData.size[i]; //находим ошибку текущей координаты//какой должен быть знак?
			linearData.err[i] += derr;
			linearData.last[i] = motor[i]._position;
		}
	}

	//=====================================================================================================
	void set_velocity()
	{
		//выбираем максимальную ошибку оси
		//и для неё округляем скорость двигателя в меньшую сторону
		int maxAxe = linearData.refCoord;//сначала считаем опорную максимальной
		int maxErr = 0; //по опорной ошибка всегда = 0

		for (int i = 0; i < NUM_COORDS; ++i)
		{
			int err = linearData.err[i];
			if (err > maxErr)
			{
				maxErr = err;
				maxAxe = i;
			}
		}

		//задаем скорости
		if (linearData.velocity.mantis != 0)
			for (int i = 0; i < NUM_COORDS; ++i)
			{
				if (linearData.velCoef[i].mantis == 0)
				{
					motor[i].set_period(MAX_STEP_TIME);
					continue;
				}
				int stepTime = linearData.velCoef[i] / linearData.velocity;
				if (i == maxAxe)
					stepTime += (stepTime >> 2) + 1; //регулировка скорости на 1 %
				if (uint32_t(stepTime) > MAX_STEP_TIME)
					stepTime = MAX_STEP_TIME;
				motor[i].set_period(stepTime);

				//if(canLog) log_console("st[%d] = %d\n", i, stepTime);
			}
			//if(canLog) log_console("maxe %d, %d\n", maxAxe, maxErr);
	}

	//=====================================================================================================
	bool brez_step()
	{
		int time = timer.get();
		for (int i = 0; i < NUM_COORDS; ++i) //быстро запоминаем текущее состояние координат
			if (linearData.size[i] != 0)
				motor[i].shot(time);

		for (int i = 0; i < NUM_COORDS; ++i)
			coord[i] = motor[i]._position;

		int ref = linearData.refCoord;
		if((coord[ref] - to[ref]) * linearData.sign[ref] >= 0) //если дошли до конца, выходим
			return false;

		//находим ошибку новых координат
		compute_error();

		//обновляем скорости двигателей
		set_velocity();
		
		return true;
	}


	//=====================================================================================================
	bool brez_init(int dest[NUM_COORDS])
	{
		bool diff = false;
		for(int i = 0; i < NUM_COORDS; ++i) //для алгоритма рисования
		{
			from[i] = to[i];       //куда должны были доехать
			to[i] = dest[i];       //куда двигаемся
			if(to[i] > from[i])
			{
				linearData.size[i] = to[i] - from[i]; //увеличение ошибки
				linearData.sign[i] = 1;                 //изменение координаты
			}
			else
			{
				linearData.size[i] = from[i] - to[i];
				linearData.sign[i] = -1;
			}

			if(to[i] != from[i])
				diff = true;
		}

		//инициализируем ошибку, учитывая, что в начале отрезка она = 0
		for(int i = 0; i < NUM_COORDS; ++i)
		{
			linearData.err[i] = 0;
			linearData.last[i] = from[i];
		}
		compute_error();

		return diff;
	}

	//=====================================================================================================
	OperateResult linear()
	{
		int reference = linearData.refCoord;

		float16 length = linearData.invProj * float16(to[reference] - coord[reference]);
		length += (float16(1) / float16(1000)) * float16(current_track_length());

		// 1/linearData.velocity = тик/мм
		float16 currentFeed = linearData.maxFeedVelocity * feedMult;
#ifdef USE_ADC_FEED
		currentFeed *= (float16(int(adc.value())) >> 12); //на максимальном напряжении   *= 0.99999
#endif
/*log_console("step %d, %d, %d, %d\n", length.mantis, length.exponent,
 currentFeed.mantis, currentFeed.exponent);*/

		int lastState = linearData.state;
		if (needStop
		// v^2 = 2g*h;
		|| (pow2(linearData.velocity) > (linearData.acceleration * length << 1))
		|| (linearData.velocity > currentFeed))
		{
			linearData.state = -1;
		}
		else if (linearData.velocity < currentFeed)
		{
			linearData.state = 1;
		}
		else
			linearData.state = 0;

		if (lastState != linearData.state)
		{
			linearData.lastVelocity = linearData.velocity;
			linearData.lastTime = timer.get();
		}

		if (lastState == -1)
		{
			int currentTime = timer.get();
			int delta = currentTime - linearData.lastTime;
			linearData.velocity = linearData.lastVelocity - float16(delta) * linearData.acceleration;
			if (delta > 100000)
			{
				linearData.lastVelocity = linearData.velocity;
				linearData.lastTime = currentTime;
			}
			if(linearData.velocity.mantis <= 0)
			{
				linearData.velocity.mantis = 0;
				linearData.velocity.exponent = 0;
			}
		}
		else if (lastState == 1)
		{
			int currentTime = timer.get();
			int delta = currentTime - linearData.lastTime;
			linearData.velocity = linearData.lastVelocity + float16(delta) * linearData.acceleration;
			if (delta > 100000)
			{
				linearData.lastVelocity = linearData.velocity;
				linearData.lastTime = currentTime;
			}
			if(linearData.velocity > currentFeed)
				linearData.velocity = currentFeed;
		}
/*log_console("step %d, %d, %d, %d, %d, %d, %d\n", linearData.velocity.mantis, linearData.velocity.exponent,
linearData.acceleration.mantis, linearData.acceleration.exponent,
linearData.state,
linearData.lastTime, timer.get()
);*/
		if (!brez_step())
		{
			if (current_track_length() == 0)
				stop_motors();
			return END;
		}

		return WAIT;
	}


	//=====================================================================================================
	void init_linear(int dest[NUM_COORDS], int refCoord, float16 acceleration, int uLength, float16 length, float16 velocity)
	{
		dec_track(uLength);

		linearData.refCoord = refCoord; //используется в brez_init

		if(!brez_init(dest)) //если двигаться никуда не надо, то выйдет на первом такте
		{
			log_console("ERR: brez %d, %d, %d\n", dest[0], dest[1], dest[2]);
			return;
		}

		start_motors();

		linearData.acceleration = acceleration;
		linearData.maxFeedVelocity = velocity;
		linearData.invProj = length / float16(linearData.size[refCoord] * linearData.sign[refCoord]);

		for (int i = 0; i < NUM_COORDS; ++i)
		{
			motor[i].set_direction(linearData.sign[i] > 0);
			//нужна скорость в тиках/шаг по оси
			//есть скорость мм/тик вдоль отрезка
			//находим проекцию на ось
			//stepLength[i]; мм/шаг
			if (linearData.size[i] == 0)
				linearData.velCoef[i].mantis = linearData.velCoef[i].exponent = 0;
			else
				linearData.velCoef[i] = length / float16(linearData.size[i]);
		}

		handler = &Mover::linear;
	}


	//=====================================================================================================
	OperateResult empty()
	{
		if((unsigned int)timer.get() % 12000000 > 6000000)
			led.show();
		else
			led.hide();

		if(receiver.queue.IsEmpty())
			return WAIT;
		else
			return END;
	}


	//=====================================================================================================
	void init_empty()
	{
		handler = &Mover::empty;
	}


	//=====================================================================================================
	OperateResult wait()
	{
		if(timer.check(stopTime))
			return WAIT;
		else
			return END;
	}


	//=====================================================================================================
	void init_wait(int delay)
	{
		stopTime = timer.get_mks(delay);
		handler = &Mover::wait;
	}


	//=====================================================================================================
	void process_packet_move(PacketMove *packet)
	{
		switch (interpolation)
		{
		case MoveMode_FAST:
		case MoveMode_LINEAR:
			{
				led.flip();

				//log_console("pos %7d, %7d, %5d, time %d init\n",
				//       packet->coord[0], packet->coord[1], packet->coord[2], timer.get());
				send_packet_service_coords(coord);
				int coord[NUM_COORDS];
				for (int i = 0; i < NUM_COORDS; ++i)
					coord[i] = packet->coord[i];
				init_linear(coord, packet->refCoord, packet->acceleration, packet->uLength, packet->length, packet->velocity);
				break;
			}
		}

		//log_console("posle1  first %d, last %d\n", receiver.queue.first, receiver.queue.last);
	}


	//=====================================================================================================
	typedef OperateResult (Mover::*Handler)();
	Handler handler;

	void update()
	{
		OperateResult result = (this->*handler)();
		if(result == END) //если у нас что-то шло и кончилось
		{
			if(receiver.queue.IsEmpty())
			{
				init_empty();
				empty();
			}
			else
			{
				const PacketCommon* common = (PacketCommon*)&receiver.queue.Front();
				send_packet_service_command(common->packetNumber);
				
				switch(common->command)
				{
				case DeviceCommand_MOVE:
					{
						process_packet_move((PacketMove*)common);
						break;
					}
				case DeviceCommand_MOVE_MODE:
					{
						interpolation = ((PacketInterpolationMode*)common)->mode;
						log_console("mode %d\n", interpolation);

						break;
					}
				case DeviceCommand_SET_BOUNDS:
				{
					//PacketSetBounds *packet = (PacketSetBounds*)common;
					break;
				}
				case DeviceCommand_SET_VEL_ACC:
				{
					PacketSetVelAcc *packet = (PacketSetVelAcc*)common;
					for(int i = 0; i < NUM_COORDS; ++i)
					{
						maxVelocity[i] = packet->maxVelocity[i];
						maxAcceleration[i] = packet->maxAcceleration[i];
						log_console("[%d]: maxVel %d, maxAcc %d\n", i, int(maxVelocity[i]), int(maxAcceleration[i]));
					}
					break;
				}
				case DeviceCommand_SET_FEED:
					{
						/*PacketSetFeed *packet = (PacketSetFeed*)common;
						linearData.feedVelocity = packet->feedVel;
						log_console("feed %d\n", int(linearData.feedVelocity));*/
						break;
					}
				case DeviceCommand_SET_STEP_SIZE:
					{
						PacketSetStepSize *packet = (PacketSetStepSize*)common;
						for(int i = 0; i < NUM_COORDS; ++i)
						{
							stepLength[i] = packet->stepSize[i];
							log_console("[%d]: stepLength %d\n", i, int(stepLength[i].exponent));
						}
						break;
					}
				case DeviceCommand_WAIT:
					{
						PacketWait *packet = (PacketWait*)common;
						init_wait(packet->delay);
						break;
					}

				default:
					log_console("ERR: undefined packet type %d, %d\n", common->command, common->packetNumber);
					break;
				}

				receiver.queue.Pop();
				//log_console("POSLE  first %d, last %d\n",
				//      receiver.queue.first, receiver.queue.last);
			}
		}
	}


	//=====================================================================================================
	void init()
	{
		for(int i = 0; i < NUM_COORDS; i++)
		{
			coord[i] = 0;
		}

		needStop = false;
		stopTime = 0;
		handler = &Mover::empty;

		//это должно задаваться с компьютера
		/*for(int i = 0; i < NUM_COORDS; i++)
		{
			const float stepSize = 0.1f; //0.1 мм на шаг
			const float mmsec = 100; //мм/сек
			const float delay = 0.000001; //1 тик - 1 микросекунда
			const float accel = 100;//мм/сек^2
			
			maxVelocity[i] = mmsec/stepSize*delay;
			maxAcceleration[i] = accel/stepSize*delay*delay;
		}*/
		interpolation = MoveMode_FAST;
		//feedVelocity = maxVelocity[0]; //для обычной подачи задержка больше
		feedMult = 1;
	}

	//=====================================================================================================
	//вызывается в потоке приёма
	void new_track()
	{
		if(receiver.tracks.IsFull())
			log_console("ERR: fracts overflow %d\n", 1);
		Track track;
		track.segments = 0;
		track.uLength = 0;
		receiver.tracks.Push(track);
		//log_console("fract %d\n", receiver.tracks.Size());
	}

	//=====================================================================================================
	//вызывается в потоке приёма перед добавлением пакета
	void add_to_track(int length)
	{
		if(receiver.tracks.IsEmpty())
		{
			Track track;
			track.segments = 0;
			track.uLength = 0;
			receiver.tracks.Push(track);
			log_console("no fract %d\n", 0);
		}
		Track *track = &receiver.tracks.Back();
		++track->segments;
		track->uLength += length;
		//log_console("move %d, %d, %d\n", track->uLength, track->segments, receiver.tracks.Size());
	}

	//=====================================================================================================
	//вызывается в основном потоке
	//на момент извлечения отрезок уже есть в линии,
	//так что число отрезков в ней != 0
	void dec_track(int uLength)
	{
		Track *track = &receiver.tracks.Front();
		__disable_irq();
		while(track->segments == 0)
		{
			//log_console("fract2 %d\n", receiver.tracks.Size());
			receiver.tracks.Pop();
			linearData.velocity = 0;         //при завершении линии сбрасываем скорость
			linearData.state = 0;
			track = &receiver.tracks.Front();
		}

		--track->segments;
		track->uLength -= uLength;
		if(track->uLength < 0) //вычитаем то, что перед этим было добавлено
			log_console("ERR: len %d, %d, %d\n", track->segments, track->uLength, uLength);
		__enable_irq();
	}

	//=====================================================================================================
	int current_track_length()
	{
		return receiver.tracks.Front().uLength;
	}
};

Mover mover;


//=====================================================================================================
void process_packet(char *common, int size)
{
	switch(((PacketCommon*)common)->command)
	{
	case DeviceCommand_SET_FEED_MULT:
		{
			PacketSetFeedMult *packet = (PacketSetFeedMult*)common;
			mover.feedMult = packet->feedMult;
			log_console("feedMult %d\n", int(mover.feedMult.exponent));
			send_packet_received(++receiver.packetNumber);
			break;
		}
	case DeviceCommand_PAUSE:
		{
			PacketPause *packet = (PacketPause*)common;
			mover.needStop = (packet->needStop != 0);
			log_console("needStop %d\n", int(mover.needStop));
			send_packet_received(++receiver.packetNumber);
			break;
		}
	default:
		{
			if(!receiver.queue.IsFull())
			{
				++(receiver.packetNumber);
				if(*common == DeviceCommand_SET_FRACT)
					mover.new_track();
				else if(*common == DeviceCommand_MOVE)
					mover.add_to_track(((PacketMove*)common)->uLength);

				if(*common != DeviceCommand_SET_FRACT)
				{
					push_received_packet(common, size); //при использовании указателей можно было бы не копировать ещё раз
					send_packet_received(receiver.packetNumber); //говорим, что приняли пакет
				}
			}
			else
				send_packet_queue_full(((PacketCommon*)common)->packetNumber);
		}
	}
}
