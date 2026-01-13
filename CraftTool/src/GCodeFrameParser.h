#pragma once

#include "IRemoteDevice.h"
#include "GCodeLexer.h"

namespace Interpreter
{
	enum class ModalGroup //некоторые операторы не могут одновременно содержаться в одном фрейме
	{
		NONE = 0, //g4,g10,g28,g30,g53,g92.[0-3]
		MOVE,                 //g0..g3 //G38.2, G80..G89
		INCREMENTAL, //g90..g91
		UNITS, //g20..g21
		FEED_MODE,              // G94, G95
		COORD_SYSTEM, //g54..g58
		TOOL_LENGTH_CORRECTION, //g43,g44,g49
		TOOL_RADIUS_CORRECTION, //g40..g42
		CYCLE_RETURN, //g98, g99
		ACTIVE_PLANE, //g17..g19
		STOP, //M0, M1, M2, M30, M60
		TOOL_CHANGE, //M6
		TURN_TOOL, //M3, M4, M5
		GREASER, //M7, M8, M9
	};

	enum class BitPos
	{
		ERR = -1,
		X=0, Y, Z,
		A, B, C,
		I, J, K,
		F, P, Q, S, R, D, L,
	};

	struct Flags
	{
		int flags;
		void reset() {flags = 0;}
		bool get(int pos)
		{
			return (flags & (1<<pos)) != 0;
		}
		void set(int pos, bool value)
		{
			if(value)
				flags |= (1<<pos);
			else
				flags &= ~(1<<pos);
		}
	};

	//параметры, прочитанные из одной строки
	struct GCodeFrameParser
	{
		static const int flags = 256/32;

		double value[32];  //значения для кодов, имеющих значения
		Flags flagValue;          //битовый массив

		//int modalCodes[12]; //по числу модальных команд
		Flags flagModal;

		bool sendWait;    //G4 readed
		Plane plane;      //G17
		UnitSystem units; //G20
		bool absoluteSet; //G53
		int coordSystemNumber; //G54
		int incremental;  //G90
		CannedCycle cycle;//G80-G83
		CannedLevel cycleLevel; //G98, G99

		MotionMode motionMode;
		FeedMode feedMode;

		GCodeFrameParser()
		{
			reset();
		}

		void reset();
		InterError make_new_state(GCodeLexer& lexer);      //чтение команд из строки
		ModalGroup get_modal_group(char letter, double value); //возвращает модальную группу команды


		BitPos get_bit_pos(char letter);

		InterError set_value(char letter, double value);   //для кодов, имеющих значение, сохраняем это значение
		bool get_value(char letter, double &value);        //забираем считанное значение
		bool have_value(char letter);                      //есть ли значение
	};
}
