//#include "math.h"
#include "common.h"

//--------------------------------------------------
int fullStep[4][2] = 
 {{ 1,0 },
  { 0,1 },
  {-1,0 },
  { 0,-1}
 }; //обычный режим шагового двигателя, полный шаг

int halfStep[8][2] =
 {{1,0},
   {1,1},
  {0,1},
   {-1,1},
  {-1,0},
   {-1,-1},
  {0,-1},
   {1,-1}
 }; //полушаг, можно было просто складывать два соседних значения для полного шага

//--------------------------------------------------
//заполняем таблицу на этапе компиляции
extern int8_t cosTable[COS_TABLE_COUNT]; //один период = 4 шагам, поэтому здесь cos от 0 до 2*Pi
//===============================================================
//всё что относится к одному экземпляру двигателя
struct Motor
{
	int index;         //номер для доступа к выводам контроллера
	int stepPhase;     //что надо прибавить к текущим координатам, чтобы получить фазу сигналов на обмотках
	int maxVoltage;    //максимальное напряжение на обмотках 0-255 (регуляция тока)
	int LRfactor;      //скорость нарастания тока при приложении постоянного напряжения, показатель экспоненты, 0-255
	int position;      //по каким координатам сейчас расположена гайка

	Motor()
	{
		index = 0;
		stepPhase = 0;
		maxVoltage = 255;
		position = 0;
	}
	//------------------------------------------------------------
	//задаёт напряжение на выводах шагового двигателя
	void set_coils_PWM(int pwm1, int pwm2)
	{
		switch (index) //особенно интересно смотреть ассемблерный код
		{
			case 0:
				set_inductor_pulse_width<0>(pwm1);
				set_inductor_pulse_width<1>(pwm2);
				break;
			case 1:
				set_inductor_pulse_width<2>(pwm1);
				set_inductor_pulse_width<3>(pwm2);
				break;
			case 2:
				set_inductor_pulse_width<4>(pwm1);
				set_inductor_pulse_width<5>(pwm2);
				break;
			case 3:
				set_inductor_pulse_width<6>(pwm1);
				set_inductor_pulse_width<7>(pwm2);
				break;
		}
	}

	//------------------------------------------------------------
	//в зависимости от позиции задаёт напряжение
	void set_sin_voltage(int position, int percent) //доля напряжения изменяется от 0 до 255
	{
		position += stepPhase;
		//первые биты отвечают за значение косинуса
		int cosIndex = position & (COS_TABLE_COUNT-1);    // 0b111... и так (DIV_BITS+2) раз
		//int quart = (position >> DIV_BITS) & (0x3);  //два бита, кодирующие четверть //не будем делать кучу ifов, пусть лежит весь период в массиве
		int sinIndex = cosIndex - COS_TABLE_COUNT / 4; //синус - это косинус, подвинутый вправо на 1/4, аргумент двигаем влево
		if (sinIndex < 0) sinIndex += COS_TABLE_COUNT;
		
		int voltage1 = PWM_SIZE * percent * cosTable[cosIndex]    / (1<<15); //если бы была гарантия, что знак не потеряется...
		int voltage2 = PWM_SIZE * percent * cosTable[sinIndex]    / (1<<15);
		
		voltage1 = voltage1 * maxVoltage /(1<<7);
		voltage2 = voltage2 * maxVoltage /(1<<7);
		
		set_coils_PWM(voltage1, voltage2);
	}

	//------------------------------------------------------------
	//задание напряжения при полном шаге
	void set_full_step_voltage(int position, int percent)
	{
		position += stepPhase;
		int quart = (position >> DIV_BITS) & (0x3);  //два бита, кодирующие четверть
		int cosIndex = fullStep[quart][0];
		int sinIndex = fullStep[quart][1];
		
		int voltage1 = PWM_SIZE * percent * cosTable[cosIndex]    / (1<<8); //если бы была гарантия, что знак не потеряется...
		int voltage2 = PWM_SIZE * percent * cosTable[sinIndex]    / (1<<8);
		
		set_coils_PWM(voltage1, voltage2);
	}
};
