#include "fifo.h"
#include "packets.h"
#include "motor.h"
#include "sys_timer.h"
#include "led.h"
#include "float16.h"

void send_packet(char *packet, int size);

class Receiver
{
	public:
	FIFOBuffer<MaxPacket, 3> queue; //28*32+8 = 904
	PacketCount packetNumber;
	
	void init()
	{
		queue.Clear();
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
void send_packet_error_number(int number)
{
	PacketErrorPacketNumber packet;
	packet.command = DeviceCommand_ERROR_PACKET_NUMBER;
	packet.packetNumber = number;
	packet.crc = calc_crc((char*)&packet, sizeof(packet) - 4);
	send_packet((char*)&packet, sizeof(packet));
}

//=========================================================================================
void push_received_packet(char *packet, int size)
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
void on_packet_received(char *packet, int size)
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
			send_packet_received(receiver.packetNumber);                       //шлём ещё раз
			
			//log_console("\nBYLO %d %d\n", common->packetNumber, receiver.packetNumber);
		}
		else if(common->packetNumber == PacketCount(receiver.packetNumber + 1)) //принят следующий пакет
		{
			if(!receiver.queue.IsFull())
			{
				++(receiver.packetNumber);
				push_received_packet(packet, size); //при использовании указателей можно было бы не копировать ещё раз
				send_packet_received(receiver.packetNumber); //говорим, что приняли пакет
  			//log_console("\nGOTOV %d %d\n", common->packetNumber, receiver.packetNumber);
			}
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
	minVal = (value/minVal + minVal)/2;
	minVal = (value/minVal + minVal)/2;
	minVal = (value/minVal + minVal)/2;
	return minVal;	
}

int iabs(int value)
{
	return value>0 ? value : -value;
}

int ipow2(int value)
{
	return value*value;
}

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
	int minCoord[NUM_COORDS]; //габариты станка
	int maxCoord[NUM_COORDS]; //должны определяться по концевым выключателям
	int maxrVelocity[NUM_COORDS];    // 1/максимальная скорость передвижения (мкс/шаг)
	int maxrAcceleration[NUM_COORDS];// 1/максимальное ускорение (мкс^2/шаг)
	float16 stepLength[NUM_COORDS];      //мм/шаг
	
	int coord[NUM_COORDS];    //текущие координаты
	int bufCoord[NUM_COORDS]; //предрассчитанные новые координаты
	int velocity[NUM_COORDS]; //текущая скорость
	bool needStop;   //принудительная остановка
	int stopTime;    //время следующей остановки
	int startTime;   //время начала текущего тика обработки

	int nextBound[NUM_COORDS];//координаты, на которых надо затормозить
	
	int from[NUM_COORDS];     //откуда двигаемся
	int to[NUM_COORDS];       //куда двигаемся
	
	MoveMode interpolation;
	int rFeed;    //подача (тиков/мм)
	int voltage[NUM_COORDS];  //регулировка напряжения на больших оборотах и при простое
	MovePlane plane;
	int remap[2];          //номера координат, соответствующие плоскости интерполяции
	
	//----------------------------------
	enum State
	{
		FAST = 0,
		LINEAR,
		CW,
		CCW,
	};
	
	//----------------------------------
	enum OperateResult
	{
		END = 1,
		WAIT,
	};
	
	//----------------------------------
	struct LinearData
	{
		int refCoord;          //индекс координаты, по которой шагаем
		int refDelta;          //разность опорной координаты
		int delta[NUM_COORDS]; //увеличение ошибки округления координат
		int err[NUM_COORDS];   //ошибка округления координат
		int add[NUM_COORDS];   //изменение координаты при превышении ошибки {-1 || 1}
		int maxrVelocity;      //максимальная скорость, тиков/шаг
		int maxrAcceleration;  //ускорение тиков^2/шаг
		int rVelocity;         //скорость на прошлом шаге
		int accLength;         //расстояние, в течение которого можно ускоряться
		int state;             //0 - ускорение, 1 - движение, 2 - торможение
	};
	LinearData linearData;
	
	//----------------------------------
	struct CircleData
	{
		int center[2];         //центр окружности
		int coord[2];          //координаты в локальной системе с началом в центре круга
		bool isCw;             //по часовой
		int error;             //накопленная ошибка в текущей позиции
		float16 scale2;        //(растяжение по y)^2
		int oldDelta;          //предыдущее расстояние до конца дуги
		float16 maxVelocity;   //максимальная скорость, мм/тик
		float16 maxAcceleration;  //ускорение мм/тик^2
		float16 accLength;     //расстояние, в течение которого можно ускоряться
		int state;             //0 - ускорение, 1 - движение, 2 - торможение
		float16 stepLenSq[2];  //квадрат длины шага, для ускорения вычислений
	};
	CircleData circleData;
	
	#define MAX_CIRCLE_DELTA 10 //максимальное непопадание конца дуги из-за неточности вычислений
	
	//----------------------------------
	bool linear_step()
	{
		if(coord[linearData.refCoord] == to[linearData.refCoord]) //дошли до конца, выходим
			return false;

		for (int i = 0; i < NUM_COORDS; ++i)
		{
			linearData.err[i] += linearData.delta[i];     //считаем ошибку
			bufCoord[i] = coord[i];
			if(2 * linearData.err[i] >= linearData.refDelta)
			{
				bufCoord[i] += linearData.add[i];           //добавляем координату
				linearData.err[i] -= linearData.refDelta;   //вычитаем ошибку
			}
		}
		return true;
	}
	
	//----------------------------------
	OperateResult linear()
	{
		int reference = linearData.refCoord;
		if(!linear_step())
			return END;

		int length; // =1/v;  v=v0+a*t
		switch(linearData.state)
		{
			case 0:
			{
				length = iabs(coord[reference] - from[reference]);
				if(length >= linearData.accLength)
					linearData.state = 1;
				if(length == 0)
					length = 1;
				linearData.rVelocity = isqrt(linearData.maxrAcceleration / (2*length));
				break;
			}
			case 1:
			{
				if(iabs(coord[reference] - to[reference]) < linearData.accLength)
					linearData.state = 2;
				linearData.rVelocity = linearData.maxrVelocity;
				break;
			}
			case 2:
			{
				length = iabs(coord[reference] - to[reference]);
				if(length == 0)
					length = 1;
				linearData.rVelocity = isqrt(linearData.maxrAcceleration / (2*length));
				break;
			}
		}

		stopTime = timer.get_mks(startTime, linearData.rVelocity);

		return WAIT;
	}
	
	//----------------------------------
	void init_linear(int dest[3], bool isMax)
	{
		bool isEqual = true;
		float16 lengths[NUM_COORDS];
		for(int i = 0; i < NUM_COORDS; ++i) //для алгоритма рисования
		{
			to[i] = dest[i];       //куда двигаемся
			from[i] = coord[i];    //откуда (текущие координаты)
			linearData.err[i] = 0; //накопленная ошибка
			if(to[i] > from[i])
			{
				linearData.delta[i] = to[i] - from[i]; //увеличение ошибки
				linearData.add[i] = 1;                 //изменение координаты
			}
			else
			{
				linearData.delta[i] = from[i] - to[i];
				linearData.add[i] = -1;
			}
			if(to[i] != from[i])
				isEqual = false;
				
			lengths[i] = linearData.delta[i];
		}
		
		if(isEqual) //если двигаться никуда не надо, то выйдет на первом такте
			return;
		
		linearData.refCoord = 0;
		linearData.refDelta = linearData.delta[0];     //находим опорную координату (максимальной длины)
		for(int i = 1; i < NUM_COORDS; ++i)
			if(linearData.refDelta < linearData.delta[i])
			{
				linearData.refDelta = linearData.delta[i];
				linearData.refCoord = i;
			}
		
		//для алгоритма ускорения
		int ref = linearData.refCoord;
		int refLen = linearData.delta[ref]; //проекция отрезка на опорную координату
		
		//находим максимальную скорость по опорной координате
		float16 timeMax = lengths[0] * float16(maxrVelocity[0]);
		for(int i = 1; i < NUM_COORDS; ++i)
		{
			float16 time = lengths[i] * float16(maxrVelocity[i]);
			if(timeMax < time)
				timeMax = time;
		}
		linearData.maxrVelocity = timeMax / float16(refLen);
		
		//ограничиваем скорость подачей
		if(!isMax)
		{
			float16 sqLen = pow2(lengths[0] * stepLength[0]); //мм
			for(int i = 1; i < NUM_COORDS; ++i)
				sqLen += pow2(lengths[i] * stepLength[i]);
		
			float16 length = sqrt(sqLen);    //длина отрезка в мм
			float16 projLen = lengths[ref] / length; //косинус скорости на опорной координате
			//тик/мм * мм/шаг 
			int projFeedVelocity = int(float16(rFeed) / projLen); //проекция скорости подачи на опорную координату
			if(linearData.maxrVelocity < projFeedVelocity) //скорость подачи < макс. достижимой
				linearData.maxrVelocity = projFeedVelocity;
		}
		
		//находим максимальное ускорение по опорной координате
		timeMax = lengths[0] * float16(maxrAcceleration[0]);
		for(int i = 1; i < NUM_COORDS; ++i)
		{
			float16 time = lengths[i] * float16(maxrAcceleration[i]);
			if(timeMax < time)
				timeMax = time;
		}
		linearData.maxrAcceleration = timeMax / lengths[ref];
		
		//теперь смотрим, достигнем ли такой скорости, когда дойдём до середины отрезка
		//v=a*t, s=a*t^2/2 =v^2/2a
		int accLength = float16(linearData.maxrAcceleration)/(float16(linearData.maxrVelocity)*float16(linearData.maxrVelocity)) ;
		accLength /= 2;
		//int accLength = linearData.maxrAcceleration/linearData.maxrVelocity;
		//accLength /= (2 * linearData.maxrVelocity);
		//log_console("acc %d, vel %d\n", linearData.maxrAcceleration, linearData.maxrVelocity);
		//log_console("len %d, len2 %d\n", accLength, refLen);
		if(accLength > refLen/2)
		{
			accLength = refLen/2;
			linearData.maxrVelocity = isqrt(linearData.maxrAcceleration / refLen);
		}
		linearData.accLength = accLength;
		linearData.state = 0;
		//log_console("len %d, acc %d, vel %d, ref %d\n", accLength, linearData.maxrAcceleration, linearData.maxrVelocity, ref);
		
		for(int i=0; i<NUM_COORDS; ++i)
		  voltage[i] = 256;
			
		handler = &Mover::linear;
	}

	//----------------------------------
	OperateResult circle()
	{
		//выбираем направление движения
		int add[2];
		add[0] = isign(circleData.coord[1]);
		add[1] = -isign(circleData.coord[0]);
		if(!circleData.isCw)
		{
			add[0] *= -1;
			add[1] *= -1;
		}
		
		//определяем, как будет меняться ошибка при изменении координат
		int delta[2];
		delta[0] = (circleData.coord[0] * 2 + add[0]) * add[0];
		delta[1] = float16(((circleData.coord[1] * 2) + add[1]) * add[1]) * circleData.scale2;
		int diagonalDelta = delta[0] + delta[1] + circleData.error;
		bool haveDelta[2] = {false, false};
		//для каждой 1/8 круга движение только по двум направлениям, например дигонально и горизонтально
		//или диагонально и вертикально
		bool horizontal = iabs(circleData.coord[0]) < iabs(float16(circleData.coord[1]) * circleData.scale2);
		if(horizontal)
		{
			circleData.coord[0] += add[0];
			haveDelta[0] = true;
			int horizontalDelta = diagonalDelta - delta[1];
			if(iabs(diagonalDelta) < iabs(horizontalDelta))
			{
				circleData.coord[1] += add[1];
				haveDelta[1] = true;
				circleData.error = diagonalDelta;
			}
			else
				circleData.error = horizontalDelta;
		}
		else
		{
			circleData.coord[1] += add[1];
			haveDelta[1] = true;
			int verticalDelta = diagonalDelta - delta[0];
			if(iabs(diagonalDelta) < iabs(verticalDelta))
			{
				circleData.coord[0] += add[0];
				haveDelta[0] = true;
				circleData.error = diagonalDelta;
			}
			else
				circleData.error = verticalDelta;
		}

		int newDelta = iabs(to[remap[0]] - coord[remap[0]]) + iabs(to[remap[1]] - coord[remap[1]]);
		if(newDelta < MAX_CIRCLE_DELTA)
		{
			if(newDelta < circleData.oldDelta)
				circleData.oldDelta = newDelta;
			else
				return END;
		}
		
		for(int i = 0; i < 2; ++i)
		{
			bufCoord[remap[i]] = circleData.center[i] + circleData.coord[i];
		}
		
		//найти точное выражение для ускорения не получилось, т.к. эллиптические интегралы
		//поэтому считать будем по расстоянию от старта до текущей позиции
		//s = v^2/2a, v = sqrt(2sa)
		int time = 0;
		float16 stepLen = 0;
		if(haveDelta[0])
			stepLen = circleData.stepLenSq[0];

		if(haveDelta[1])
			stepLen += circleData.stepLenSq[1]; //здесь корень не берём для оптимизации

		int curPos[2] = {bufCoord[remap[0]], bufCoord[remap[1]]};

		switch(circleData.state)
		{
			case 0:
			{
				float16 distanceToStart = sqrt(
						pow2(curPos[0] - from[remap[0]]) * circleData.stepLenSq[0] +
						pow2(curPos[1] - from[remap[1]]) * circleData.stepLenSq[1]);
				//v = sqrt(2sa)   dt = dl / v = sqrt(dl2 /(2sa))
				//v = Vmax*sqrt(s/Smax)
				time = sqrt(stepLen / (distanceToStart * circleData.maxAcceleration << 1));
				if(distanceToStart > circleData.accLength) //отъехали от начала
				{
					circleData.state = 1;
					//log_console("to state 1,  %d, %d\n", distanceToStart.mantis, distanceToStart.exponent);
				}
				break;
			}
			case 1:
			{
				float16 distanceToStop = sqrt(
							pow2(curPos[0] - to[remap[0]]) * circleData.stepLenSq[0] + 
							pow2(curPos[1] - to[remap[1]]) * circleData.stepLenSq[1]);
				//dt = dl / v
				time = sqrt(stepLen) / circleData.maxVelocity;
				if(distanceToStop < circleData.accLength) //подъехали к концу
				{
					circleData.state = 2;
					//log_console("to state 2,  %d, %d\n", distanceToStop.mantis, distanceToStop.exponent);
				}
				break;
			}
			case 2:
			{
				float16 distanceToStop = sqrt(
							pow2(curPos[0] - to[remap[0]]) * circleData.stepLenSq[0] + 
							pow2(curPos[1] - to[remap[1]]) * circleData.stepLenSq[1]);
				//dt = sqrt(dl2 /(2sa))
				time = sqrt(stepLen / (distanceToStop * circleData.maxAcceleration << 1));
				break;
			}
		}
		
		stopTime = timer.get_mks(startTime, time);
		
		return WAIT;
	}
	
	//----------------------------------
	void init_circle(int dest[3], int center[3], bool isCw)
	{
		for(int i = 0; i < NUM_COORDS; ++i)
			from[i] = coord[i];    //откуда (текущие координаты)
		
		circleData.isCw = isCw;
			
		for(int i = 0; i < 2; ++i)
		{
			circleData.coord[i] = coord[remap[i]]; //берём текущие координаты
			circleData.coord[i] -= center[remap[i]]; //переводим в локальные
			circleData.center[i] = center[remap[i]];
		}
		
		//круг надо отмасштабировать чтобы соответствовал масштабу осей
		float16 scale = stepLength[remap[1]] / stepLength[remap[0]];
		circleData.scale2 = scale * scale;
		
		circleData.error = 0; //мы на точке окружности, откуда тут ошибка
		circleData.oldDelta = MAX_CIRCLE_DELTA;
		circleData.state = 0; //ускоряемся
		
		for(int i = 0; i < NUM_COORDS; ++i)
			to[i] = dest[i];       //куда двигаемся

		//расчёт ускорения
		float16 lengths[2] =
		{
			to[remap[0]] - from[remap[0]],
			to[remap[1]] - from[remap[1]]
		};

		//ускорение берётся минимальное по оси, центробежная сила не учитывается
		float16 acc0 = stepLength[remap[0]] / float16(maxrAcceleration[remap[0]]); //макс. ускорение по оси
		float16 acc1 = stepLength[remap[1]] / float16(maxrAcceleration[remap[1]]); //мм/мкс^2
		float16 accel = acc0;
		if(accel > acc1)
			accel = acc1;
		circleData.maxAcceleration = accel;

		//не возился с точным вычислением скорости, берётся минимальная из двух
		float16 vel0 = stepLength[remap[0]] / float16(maxrVelocity[remap[0]]); //макс. скорость по оси
		float16 vel1 = stepLength[remap[1]] / float16(maxrVelocity[remap[1]]); //мм/тик
		
		float16 feedVel = float16(1) / float16(rFeed); //скорость подачи
		if(feedVel > vel0)
			feedVel = vel0;
		if(feedVel > vel1)
			feedVel = vel1;
			
		circleData.maxVelocity = feedVel;
		
		float16 sqLen  = pow2(lengths[0] * stepLength[remap[0]]);
			      sqLen += pow2(lengths[1] * stepLength[remap[1]]);
	
		float16 length = sqrt(sqLen) >> 1; //расстояние между концами дуги в мм
		
		//v=a*t, s=a*t^2/2 =v^2/2a
		circleData.accLength = (pow2(feedVel) / accel) >> 1;
		
		if(circleData.accLength > length)
		{
			circleData.accLength = length;
			//v = sqrt(2sa)
			circleData.maxVelocity = sqrt((length * circleData.maxAcceleration) << 1);
		}
		//log_console("acclen %d %d\n", circleData.accLength.mantis, circleData.accLength.exponent);
		//log_console("len %d, vel %d\n", int(circleData.accLength), int(circleData.maxVelocity * float16(1000000)));
		circleData.stepLenSq[0] = pow2(stepLength[remap[0]]);
		circleData.stepLenSq[1] = pow2(stepLength[remap[1]]);
		
		for(int i=0; i<NUM_COORDS; ++i)
		  voltage[i] = 256;
			
		handler = &Mover::circle;
	}
	
	//----------------------------------	
	OperateResult empty()
	{
		if((unsigned int)timer.get() % 12000000 > 6000000)
			led.show();
		else
			led.hide();
			
		if(receiver.queue.IsEmpty())
		{
			stopTime = timer.get_ms(10);
			return WAIT;
		}
		else
			return END;
	}
	
	//----------------------------------
	void init_empty()
	{
		handler = &Mover::empty;
		
		for(int i=0; i<NUM_COORDS; ++i)
		  voltage[i] = 64;
	}
	
	//----------------------------------
	void set_plane(MovePlane _plane)
	{
		plane = _plane;
		//определяем индексы интерполируемых координат
		switch(plane)
		{
			case MovePlane_XY:
				remap[0] = 0;
				remap[1] = 1;
				break;
			case MovePlane_ZX:
				remap[0] = 2;
				remap[1] = 0;
				break;
			case MovePlane_YZ:
				remap[0] = 1;
				remap[1] = 2;
				break;
		}
	}
	
	//----------------------------------
	typedef OperateResult (Mover::*Handler)();
	Handler handler;
	
	void update()
	{
		startTime = timer.get();
		motor[0].set_sin_voltage(bufCoord[0], voltage[0]);
		motor[1].set_sin_voltage(bufCoord[0], voltage[0]);
		motor[2].set_sin_voltage(bufCoord[1], voltage[1]);
		motor[3].set_sin_voltage(bufCoord[2], voltage[2]);
		
		for(int i = 0; i < NUM_COORDS; ++i)
			coord[i] = bufCoord[i];
		
		OperateResult result = (this->*handler)();
		if(result == END /* && this->handler != &Mover::empty*/) //если у нас что-то шло и кончилось
		{
			//led.hide();
			if(receiver.queue.IsEmpty())
			{
				init_empty();
				empty();
			}
			else
			{
			  //log_console("DO  first %d, last %d\n", receiver.queue.first, receiver.queue.last);	

				const PacketCommon* common = (PacketCommon*)&receiver.queue.Front();
				send_packet_service_command(common->packetNumber);
				//log_console("queue[%d] = %d\n", receiver.queue.first, common->command);
				switch(common->command)
				{
					case DeviceCommand_MOVE:
					{
						switch (interpolation)
						{
							case MoveMode_FAST:
							case MoveMode_LINEAR:
							{
								PacketMove *packet = (PacketMove*)common;
								led.flip();

			//log_console("pos %7d, %7d, %5d, time %d init\n",
			//        packet->coord[0], packet->coord[1], packet->coord[2], timer.get());
								send_packet_service_coords(coord);
								init_linear(packet->coord, interpolation == MoveMode_FAST);
								break;
							}
							case MoveMode_CW_ARC:
							case MoveMode_CCW_ARC:
							{
								PacketCircleMove *packet = (PacketCircleMove*)common;
								led.flip();
								
			//log_console("pos %d, %d, %d, %d, %d, %d, time %d init\n",
			//        packet->coord[0], packet->coord[1], packet->coord[2],
			//				packet->center[0], packet->center[1], packet->center[2], timer.get());
								send_packet_service_coords(coord);
								init_circle(packet->coord, packet->center, interpolation == MoveMode_CW_ARC);
								break;
							}
						}

			      //log_console("posle1  first %d, last %d\n", receiver.queue.first, receiver.queue.last);
						break;
					}
					case DeviceCommand_MOVE_MODE:
					{
						interpolation = ((PacketInterpolationMode*)common)->mode;
						log_console("mode %d\n", interpolation);
            //log_console("posle2  first %d, last %d\n",
			      //     receiver.queue.first, receiver.queue.last);
						
						break;
					}
					case DeviceCommand_SET_BOUNDS:
					{
						PacketSetBounds *packet = (PacketSetBounds*)common;
						for(int i = 0; i < NUM_COORDS; ++i)
						{
							minCoord[i] = packet->minCoord[i];
							maxCoord[i] = packet->maxCoord[i];
						}
						break;
					}
					case DeviceCommand_SET_VEL_ACC:
					{
						PacketSetVelAcc *packet = (PacketSetVelAcc*)common;
						for(int i = 0; i < NUM_COORDS; ++i)
						{
							maxrVelocity[i] = packet->maxrVelocity[i];
							maxrAcceleration[i] = packet->maxrAcceleration[i];
							log_console("[%d]: maxVel %d, maxAcc %d\n", i, maxrVelocity[i], maxrAcceleration[i]);
						}
						break;
					}
					case DeviceCommand_SET_FEED:
					{
						PacketSetFeed *packet = (PacketSetFeed*)common;
						rFeed = packet->rFeed;
						log_console("rFeed %d\n", rFeed);
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
					case DeviceCommand_SET_VOLTAGE:
					{
						PacketSetVoltage *packet = (PacketSetVoltage*)common;
						for(int i = 0; i < NUM_COORDS; ++i)
						{
							motor[i+1].maxVoltage = packet->voltage[i];
						}
						motor[0].maxVoltage = packet->voltage[0];
						break;
					}
					case DeviceCommand_SET_PLANE:
					{
						PacketSetPlane *packet = (PacketSetPlane*)common;
						set_plane(packet->plane);
						break;
					}
					case DeviceCommand_WAIT:
					{
						init_empty();
						PacketWait *packet = (PacketWait*)common;
						stopTime = timer.get_mks(packet->delay);
						break;
					}
						
					default:
						log_console("undefined packet type %d, %d\n", common->command, common->packetNumber);
						break;
				}
				
				receiver.queue.Pop();
				//log_console("POSLE  first %d, last %d\n",
				//      receiver.queue.first, receiver.queue.last);
			}
		}
	}
	
	//----------------------------------
	void init()
	{
		for(int i = 0; i < NUM_COORDS; i++)
		{
			minCoord[i] = 0;
			maxCoord[i] = 0;
			coord[i] = 0;
			bufCoord[i] = 0;
			velocity[i] = 0;
			nextBound[i] = 0;
		}
		
		needStop = false;
		stopTime = 0;
		handler = &Mover::empty;
		
		//это должно задаваться с компьютера
		for(int i = 0; i < NUM_COORDS; i++)
		{
			const float stepSize = 0.1f / SUB_STEPS; //0.1 мм на шаг
			const float mmsec = 100; //мм/сек
			const float delay = 0.000001; //1 тик - 1 микросекунда
			const float accel = 100;//мм/сек^2
			
			maxrVelocity[i] = 1/((mmsec/stepSize)*delay);
			maxrAcceleration[i] = 1/(accel/stepSize*delay*delay);
		}
		interpolation = MoveMode_FAST;
		rFeed = maxrVelocity[0] * 5; //для обычной подачи задержка больше
		
		set_plane(MovePlane_XY);
		
		for(int i=0; i<NUM_COORDS; ++i)
		  voltage[i] = 64;
	}
};

Mover mover;
