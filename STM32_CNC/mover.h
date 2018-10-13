#include "fifo.h"
#include "packets.h"
#include "motor.h"
#include "sys_timer.h"
#include "led.h"
#include "float16.h"

void send_packet(char *packet, int size);
void preprocess_command(PacketCommon *p);

void send_packet_error_number(int number);
void send_packet_queue_full(int number);
void send_packet_repeat(int number, int queue);
void send_packet_received(int number, int queue);

struct Track
{
	int segments;  //число отрезков в траектории
	int uLength;  //общая длина пути в микронах
};

class Receiver
{
public:
	FIFOBuffer<MaxPacket, 5> queue; //48*32+8 = 904
	FIFOBuffer<Track, 5> tracks;
	int tail; //первый ожидаемый элемент очереди (он не заполнен, а дальше могут быть заполненные)
	PacketCount index; //номер его пакета
	static const int BUFFER_LEN = 5;
	PacketCount index2; //второе соединение, обработка на лету

	void init()
	{
		queue.Clear();
		tracks.Clear();
		index = PacketCount(-1);
		index2 = PacketCount(-1);
		tail = queue.last;
	}
	
	bool queue_empty()
	{
		return queue.IsEmpty() || !queue.Front().fill;
	}
	
	void reinit()
	{
		//оставляем только непрерывный диапазон дошедших пакетов, отбросив все после первого не дошедшего
		queue.last = tail;
		index = PacketCount(0);
		index2 = PacketCount(-1);
	}

	void save_received_packet(char * __restrict packet, int size, int pos)
	{
		int size4 = (size+3)/4; //копируем сразу по 4 байта
		int *src = (int*)packet;
		int *dst = (int*)&queue.Element(pos);
		for(int i=0; i<size4; ++i)
			dst[i] = src[i];
		((MaxPacket*)dst)->fill = 1;
	}

	void packet_received(PacketCommon *p, int size)
	{
		int offset = PacketCount(p->packetNumber - index);
		if (offset < 0)
		{
			send_packet_repeat(p->packetNumber, 0);
			return;
		}
		if (offset > BUFFER_LEN)
		{
			send_packet_error_number(p->packetNumber);
			return;
		}
		int target = tail + offset;
		while(queue.last <= target) //добавляем в очередь пустые пакеты перед этим пакетом
		{
			if (queue.IsFull())
			{
				send_packet_queue_full(p->packetNumber);
				return;
			}
			MaxPacket* element = &queue.End();
			element->fill = 0;
			queue.Push();
		}
		if (queue.Element(target).fill)
		{
			send_packet_repeat(p->packetNumber, 0);
			return;
		}
		save_received_packet((char*)p, size, target);
		send_packet_received(p->packetNumber, 0); //говорим, что приняли пакет
		
		//обрабатываем принятый сплошной список пакетов
		while(queue.Element(tail).fill && (tail < queue.last))
		{
			preprocess_command((PacketCommon*)&queue.Element(tail));
			++tail;
			++index;
		}
	}

	bool packet_received2(PacketCommon *p)
	{
		if (p->packetNumber == PacketCount(index2 - 1)) //до хоста не дошёл ответ о принятом пакете
		{
			send_packet_repeat(p->packetNumber, 1);         //шлём ещё раз
		}
		else if(p->packetNumber != index2) //принят следующий пакет
		{
			send_packet_error_number(index2);
		}
		else
		{
			send_packet_received(++index2, 1);
			return true;
		}
		return false;
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
void send_packet_received(int number, int queue)
{
	PacketReceived packet;
	packet.command = DeviceCommand_PACKET_RECEIVED;
	packet.packetNumber = number;
	packet.queue = queue;
	packet.crc = calc_crc((char*)&packet, sizeof(packet) - 4);
	send_packet((char*)&packet, sizeof(packet));
}


//=========================================================================================
void send_packet_repeat(int number, int queue)
{
	PacketReceived packet;
	packet.command = DeviceCommand_PACKET_REPEAT;
	packet.packetNumber = number;
	packet.queue = queue;
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
void send_packet_service_coords(int coords[MAX_AXES])
{
	PacketServiceCoords packet;
	packet.command = DeviceCommand_SERVICE_COORDS;
	for(int i = 0; i < MAX_AXES; ++i)
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
	float16 maxVelocity[MAX_AXES];      // мм/мкс
	float16 maxAcceleration[MAX_AXES];  // мм/мкс^2
	float16 stepLength[MAX_AXES]; // мм/шаг

	int coord[MAX_AXES];    //текущие координаты
	int stopTime;             //время следующей остановки

	int from[MAX_AXES];     //откуда двигаемся
	int to[MAX_AXES];       //куда двигаемся

	MoveMode interpolation;
	bool needPause;           //сбросить скорость до 0
	char breakState;          //сбросить очередь команд
	float16 feedMult;         //заданная из интерфейса скорость движения

	//данные, отвечающие за раздельное движение по осям (концевики и ручное управление)
	char limitSwitchMin[MAX_AXES]; //концевики
	char limitSwitchMax[MAX_AXES];
	char activeSwitch[MAX_AXES]; //концевики, в сторону которых сейчас едем
	char activeSwitchCount;      //количество активных концевиков
	
	char homeSwitch[MAX_AXES]; //датчики дома
	float16 axeVelocity[MAX_AXES]; //скорость оси во время попадания в концевик
	int timeActivate[MAX_AXES]; //время начала остановки
	bool homeActivated[MAX_AXES]; //при наезде на дом выставляем бит
	bool homeReached;

bool canLog;

	//расчёт ускорения не совсем корректен
	//при движении по кривой максимальное ускорение меняется из-за изменения проекции на оси
	//и расстояние торможения неизвестно
	//но на малых скоростях это не должно проявляться
	//поскольку расстояние торможения маленькое
	//и касательная к кривой поворачивается не сильно

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
	1 шаг/40 сек
	1 000 000 шаг/сек   (если 0.0001 мм/шаг)

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
		int size[MAX_AXES];  //размеры линии
		int err[MAX_AXES];   //ошибка координат (внутреннее представление)
		int error[MAX_AXES]; //ошибка координат
		int sign[MAX_AXES];  //изменение координаты при превышении ошибки {-1 || 1}
		int last[MAX_AXES+1];   //последние координаты, для которых была посчитана ошибка
		float16 velCoef[MAX_AXES]; //на что умножить скорость, чтобы получить число тактов на шаг

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
	//наезд на хард лимит
	bool switch_reached()
	{
		//при обычной езде при налете на концевик прекращаем выдавать STEP
		if (interpolation == MoveMode_LINEAR)
		{
			for (int i = 0; i < activeSwitchCount; ++i)
				if (activeSwitch[i] != -1 && get_pin(activeSwitch[i]))
				{
					log_console("switch %d, %d reached", i, activeSwitch[i]);
					return true;
				}
		}
		//при поиске дома датчик дома может быть и хард лимитом
		else if (interpolation == MoveMode_HOME)
		{
			for (int i = 0; i < MAX_AXES; ++i)
				if (linearData.size[i] != 0)
					if (activeSwitch[i] != -1 && //если концевик задан, и не равен концевику дома
						homeSwitch[i] != activeSwitch[i] && get_pin(activeSwitch[i])) //и сработал
						return true;
		}
		return false;
	}

	//=====================================================================================================
	//наезд на дом
	bool home_reached()
	{
		return (interpolation == MoveMode_HOME) && homeReached;
	}
	
	//=====================================================================================================
	void start_motors()
	{
		if (!linearData.enabled)
		{
			for (int i = 0; i < MAX_AXES; ++i)
				motor[i].start(MAX_STEP_TIME);
			virtualAxe.start(MAX_STEP_TIME);
			linearData.enabled = true;
		}
	}

	//=====================================================================================================
	void stop_motors()
	{
		if (linearData.enabled)
		{
			for (int i = 0; i < MAX_AXES; ++i)
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
		int dx = -(virtualAxe._position - linearData.last[MAX_AXES]); //находим изменение опорной координаты
		linearData.last[MAX_AXES] = virtualAxe._position;
		for (int i = 0; i < MAX_AXES; ++i)
		{
			int dy = (motor[i]._position - linearData.last[i]) * linearData.sign[i]; //находим изменение текущей координаты
			int derr = dy * linearData.size[ref] - dx * linearData.size[i]; //находим ошибку текущей координаты//какой должен быть знак?
			linearData.err[i] += derr;
			linearData.last[i] = motor[i]._position;
		}
	}

	//=====================================================================================================
	void floor_error()
	{
		int ref = linearData.refCoord;
		for (int i = 0; i < MAX_AXES; ++i)
		{
			int floorAdd = linearData.size[ref] / 2 * isign(linearData.err[i]);
			linearData.error[i] = (linearData.err[i] + floorAdd) / linearData.size[ref];
		}
	}
	
	//=====================================================================================================
	/*
		один апдейт занимает T=3000 тактов в дебаге
		время шага t=1000, ошибка e=1
		надо сделать лишний шаг за эти 3000 сек
		для этого изменим время шага
		число шагов s=T/t между апдейтами
		новое число шагов s1=T/t-e
		t1=T/s1=T/(T/t-e)=Tt/(T-t*e)=t/(1-t*e/T)~=t*(1+t*e/T)
	*/
	void set_velocity()
	{
		//задаем скорости
		if (linearData.velocity.mantis == 0)
		{
			for (int i = 0; i < MAX_AXES; ++i)
				motor[i].set_period(MAX_STEP_TIME);
			virtualAxe.set_period(MAX_STEP_TIME);
		}
		else
		{
			int ref = linearData.refCoord;
			int curTime = timer.get();
			int stepTimeArr[MAX_AXES+1];
			float16 errCoef = float16(1.0f) / float16(linearData.size[ref]);
			homeReached = true;
			for (int i = 0; i < MAX_AXES; ++i)
			{
				if (linearData.velCoef[i].mantis == 0)
				{
					stepTimeArr[i] = MAX_STEP_TIME;
					continue;
				}
				else if (interpolation == MoveMode_HOME)
				{
					//если не задали концевик, но пытаемся на него наехать, то считаем что уже на нем
					if (homeSwitch[i] == -1 || get_pin(homeSwitch[i]))
					{
						if (!homeActivated[i])
						{
							axeVelocity[i] = linearData.velocity / linearData.velCoef[i];
							timeActivate[i] = curTime;
							homeActivated[i] = true;
						}
						
						if (curTime - timeActivate[i] > (1<<30)) //страховка на случай переполнения таймера
							timeActivate[i] = curTime - (1<<30);
							
						float16 vel = axeVelocity[i] - maxAcceleration[i] * float16(curTime - timeActivate[i]);
						int stepTime = float16(1) / vel;
						if (stepTime > MAX_STEP_TIME || stepTime <= 0)
							stepTime = MAX_STEP_TIME;
						stepTimeArr[i] = stepTime;
						if (vel.mantis >= 0)
							homeReached = false;
						continue;
					}
				}

				float16 step = linearData.velCoef[i] / linearData.velocity;//linearData.maxFeedVelocity;//
				int err = linearData.err[i];
				float16 add = step * step * float16(err) * errCoef / float16(1024); //t*(1+t*e/T)
				float16 maxAdd = step * float16(0.1f); //регулировка скорости максимально на 10 %
				if (abs(add) > maxAdd)
					add = maxAdd * float16(sign(add));
				int stepTime = step + add;
				
				if (stepTime > MAX_STEP_TIME || stepTime <= 0) //0 возможен при переполнении разрядов
					stepTime = MAX_STEP_TIME;
				stepTimeArr[i] = stepTime;
			}
			//для виртуальной оси тоже считаем скорость
			{
				int stepTime = linearData.velCoef[ref] / linearData.velocity;//linearData.maxFeedVelocity;//
				if (stepTime > MAX_STEP_TIME || stepTime <= 0) //0 возможен при переполнении разрядов
					stepTime = MAX_STEP_TIME;
				stepTimeArr[MAX_AXES] = stepTime;
			}
			
			for (int i = 0; i < MAX_AXES; ++i)
			{
				motor[i].set_period(stepTimeArr[i]);

				//if(canLog) log_console("st[%X] = %d\n", i, stepTimeArr[i]);
			}
			virtualAxe.set_period(stepTimeArr[MAX_AXES]);
			//if(canLog) log_console("st[v] = %d\n", stepTimeArr[MAX_AXES]);
		}
	}

	//=====================================================================================================
	bool brez_step()
	{
		int time = timer.get();
		for (int i = 0; i < MAX_AXES; ++i) //быстро запоминаем текущее состояние координат
			if (linearData.size[i] != 0 && motor[i]._isHardware)
				motor[i].shot(time);
		virtualAxe.shot(time); //виртуальная ось самая большая, если она = 0, то выйдет еще при инициализации
			
		//находим ошибку новых координат
		compute_error();
		
		int ref = linearData.refCoord;
		
		for (int i = 0; i < MAX_AXES; ++i) //медленными осями шагаем точно
			if (linearData.size[i] != 0 && !motor[i]._isHardware)
				if (motor[i].can_step(time) && linearData.err[i] < -linearData.size[ref] / 2)
				{
					motor[i].one_step(time);
					linearData.err[i] += linearData.size[ref];
					linearData.last[i] += linearData.sign[i];
				}
		
		floor_error();
		
		for (int i = 0; i < MAX_AXES; ++i)
			coord[i] = motor[i]._position;

		//обновляем скорости двигателей
		set_velocity();
		
		if(virtualAxe._position <= 0) //если дошли до конца, выходим
			return false;
			
		return true;
	}


	//=====================================================================================================
	bool brez_init(int dest[MAX_AXES])
	{
		bool diff = false;
		for(int i = 0; i < MAX_AXES; ++i) //для алгоритма рисования
		{
			from[i] = to[i];       //куда должны были доехать
			if (interpolation != MoveMode_HOME)
				to[i] = dest[i];       //куда двигаемся
			else
				to[i] += dest[i]; //инкрементальный режим для хоминга, так как текущие координаты на компьютере могут быть сбиты
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

		int ref = linearData.refCoord;
		virtualAxe._position = (to[ref] - coord[ref]) * linearData.sign[ref];
		//инициализируем ошибку, учитывая, что в начале отрезка она = 0
		for(int i = 0; i < MAX_AXES; ++i)
		{
			linearData.err[i] = 0;
			linearData.last[i] = from[i];
		}
		linearData.last[MAX_AXES] = linearData.size[ref];
		compute_error();

		return diff;
	}

	//=====================================================================================================
	void start_acceleration()
	{
		linearData.lastVelocity = linearData.velocity;
		linearData.lastTime = timer.get();
	}

	//=====================================================================================================
	void accelerate(int multiplier)
	{
		int currentTime = timer.get();
		int delta = currentTime - linearData.lastTime;
		linearData.velocity = linearData.lastVelocity + float16(delta * multiplier) * linearData.acceleration;
		if (delta > 100000)
		{
			linearData.lastVelocity = linearData.velocity;
			linearData.lastTime = currentTime;
		}
	}
	
	//=====================================================================================================
	OperateResult linear()
	{
		if (switch_reached())
		{
			stop_motors(); //TODO сделать обработку остановки
			return END;
		}
		
		if (home_reached())
		{
			stop_motors();
			return END;
		}
			
		float16 length = linearData.invProj * float16(virtualAxe._position);
		length += (float16(1) / float16(1000)) * float16(current_track_length());

		float16 currentFeed = linearData.maxFeedVelocity * feedMult;
#ifdef USE_ADC_FEED
		currentFeed *= (float16(int(adc.value())) >> 12); //на максимальном напряжении   *= 0.99999
#endif

		int lastState = linearData.state;
		//v^2 = 2g*h; //сначала проверяем, что не врежемся с разгона
		if (pow2(linearData.velocity) > (linearData.acceleration * length << 1))
		{
			linearData.state = -2;
			//v = sqrt(2*g*h)
			linearData.velocity = sqrt(linearData.acceleration * length << 1);
		}
		else if (needPause
		|| (linearData.velocity > currentFeed))
		{
			linearData.state = -1;
			if (lastState != linearData.state)
				start_acceleration();
			else
				accelerate(-1);
			if(linearData.velocity.mantis <= 0)
				linearData.velocity = float16(0, 0);
		}
		else if (linearData.velocity < currentFeed)
		{
			linearData.state = 1;
			if (lastState != linearData.state)
				start_acceleration();
			else
				accelerate(1);
			if(linearData.velocity > currentFeed)
				linearData.velocity = currentFeed;
		}
		else
			linearData.state = 0;

		if (!brez_step())
		{
			if (current_track_length() == 0)
			{
				stop_motors();
				linearData.state = 0;
			}
			return END;
		}
		
		//прерывание обработки
		if (linearData.velocity.mantis == 0 && breakState == 1)
		{
			stop_motors();
			linearData.state = 0;
			for(int i = 0; i < MAX_AXES; ++i)
				to[i] = coord[i];
			receiver.init();
			breakState = 2;
			return END;
		}

		return WAIT;
	}


	//=====================================================================================================
	void init_linear(int dest[MAX_AXES], int refCoord, float16 acceleration, int uLength, float16 length, float16 velocity)
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
		activeSwitchCount = 0;

		for (int i = 0; i < MAX_AXES; ++i)
		{
			motor[i].set_direction(linearData.sign[i] > 0);
			
			if (linearData.size[i] == 0)
				linearData.velCoef[i].mantis = linearData.velCoef[i].exponent = 0;
			else
			{
				linearData.velCoef[i] = length / float16(linearData.size[i]);
				
				int lim = linearData.sign[i] > 0 ? limitSwitchMax[i] : limitSwitchMin[i];
				if (interpolation == MoveMode_HOME)
						activeSwitch[i] = lim; //при движении домой торможение раздельное
				else
					if (lim != -1)
						activeSwitch[activeSwitchCount++] = lim; //иначе нас интересует любое срабатывание
				homeActivated[i] = false;
			}
		}
		
		linearData.invProj = linearData.velCoef[refCoord];
		homeReached = false;
		
		handler = &Mover::linear;
	}


	//=====================================================================================================
	OperateResult empty()
	{
		if((unsigned int)timer.get() % 12000000 > 6000000)
			led.show();
		else
			led.hide();

		if (breakState == 1)
		{
			receiver.init();
			breakState = 2;
		}
	
		if(receiver.queue_empty())
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
		if (breakState == 1)
		{
			receiver.init();
			breakState = 2;
			return END;
		}
		
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
		case MoveMode_LINEAR:
		case MoveMode_HOME:
			{
				led.flip();

				//log_console("pos %7d, %7d, %5d, time %d init\n",
				//       packet->coord[0], packet->coord[1], packet->coord[2], timer.get());
				//send_packet_service_coords(coord); //надо для отладки
				int coord[MAX_AXES];
				for (int i = 0; i < MAX_AXES; ++i)
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
			if(receiver.queue_empty())
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
				case DeviceCommand_SET_VEL_ACC:
				{
					PacketSetVelAcc *packet = (PacketSetVelAcc*)common;
					for(int i = 0; i < MAX_AXES; ++i)
					{
						maxVelocity[i] = packet->maxVelocity[i];
						maxAcceleration[i] = packet->maxAcceleration[i];
						log_console("[%d]: maxVel %d, maxAcc %d\n", i, int(maxVelocity[i]), int(maxAcceleration[i]));
					}
					break;
				}
				case DeviceCommand_SET_SWITCHES:
				{
					PacketSetSwitches *packet = (PacketSetSwitches*)common;
					char *pins = 0;
					switch (packet->group)
					{
						case SwitchGroup_MIN: pins = limitSwitchMin; break;
						case SwitchGroup_MAX: pins = limitSwitchMax; break;
						case SwitchGroup_HOME: pins = homeSwitch; break;
					}
					for (int i = 0; i < MAX_AXES; ++i)
						pins[i] = packet->pins[i];
					polarity = packet->polarity;
					break;
				}
				case DeviceCommand_SET_COORDS:
				{
					PacketSetCoords *packet = (PacketSetCoords*)common;
					for (int i = 0; i < MAX_AXES; ++i)
						if (packet->used | (1 << i))
						{
							coord[i] = packet->coord[i];
							motor[i]._position = coord[i];
							to[i] = coord[i];
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
						for(int i = 0; i < MAX_AXES; ++i)
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
				case DeviceCommand_SET_FRACT:
					break;

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
		for(int i = 0; i < MAX_AXES; i++)
		{
			coord[i] = 0;
			limitSwitchMin[i] = -1;
			limitSwitchMax[i] = -1;
			homeSwitch[i] = -1;
		}

		needPause = false;
		breakState = 0;
		stopTime = 0;
		handler = &Mover::empty;

		//это должно задаваться с компьютера
		/*for(int i = 0; i < MAX_AXES; i++)
		{
			const float stepSize = 0.1f; //0.1 мм на шаг
			const float mmsec = 100; //мм/сек
			const float delay = 0.000001; //1 тик - 1 микросекунда
			const float accel = 100;//мм/сек^2
			
			maxVelocity[i] = mmsec/stepSize*delay;
			maxAcceleration[i] = accel/stepSize*delay*delay;
		}*/
		interpolation = MoveMode_LINEAR;
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
void preprocess_command(PacketCommon *p)
{
	switch(p->command)
	{
	case DeviceCommand_SET_FRACT:
		{
			mover.new_track();
			break;
		}
	case DeviceCommand_MOVE:
		{
				mover.add_to_track(((PacketMove*)p)->uLength);
				break;
		}
	}
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
		
	PacketCommon* common = (PacketCommon*)packet;
	switch (common->command)
	{
		case DeviceCommand_RESET_PACKET_NUMBER:
		{
			receiver.reinit();
			send_packet_received(-1, 0);
			break;
		}
		//--------------------------
		case DeviceCommand_SET_FEED_MULT:
		{
			if (receiver.packet_received2(common))
			{
				PacketSetFeedMult *packet = (PacketSetFeedMult*)common;
				mover.feedMult = packet->feedMult;
				log_console("feedMult %d\n", int(mover.feedMult.exponent));
			}
			break;
		}
		case DeviceCommand_PAUSE:
		{
			if (receiver.packet_received2(common))
			{
				PacketPause *packet = (PacketPause*)common;
				mover.needPause = (packet->needPause != 0);
				log_console("pause %d\n", int(mover.needPause));
			}
			break;
		}
		case DeviceCommand_BREAK:
		{
			if (mover.breakState == 2)
			{
				send_packet_received(common->packetNumber, 1);
				mover.breakState = 0;
				mover.needPause = 0;
			}
			else if (mover.breakState == 0)
			{
				mover.breakState = 1;
				receiver.reinit();
				mover.needPause = 1;
				log_console("break %d\n", common->packetNumber);
			}
			break;
		}
		//--------------------------
		default:
		{
			receiver.packet_received(common, size);
		}
	}
}
