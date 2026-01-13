#include "GCodeFrameParser.h"
#include "log.h"

using namespace Interpreter;

//====================================================================================================
BitPos GCodeFrameParser::get_bit_pos(char letter)
{
	switch (letter)
	{
		case 'X': return BitPos::X;
		case 'Y': return BitPos::Y;
		case 'Z': return BitPos::Z;
		case 'A': return BitPos::A;
		case 'B': return BitPos::B;

		case 'I': return BitPos::I;
		case 'J': return BitPos::J;
		case 'K': return BitPos::K;

		case 'F': return BitPos::F;
		case 'P': return BitPos::P;
		case 'Q': return BitPos::Q;
		case 'S': return BitPos::S;
		case 'R': return BitPos::R;
		case 'D': return BitPos::D;
		case 'L': return BitPos::L;

		default: return BitPos::ERR;
	}
}

//====================================================================================================
InterError GCodeFrameParser::set_value(char letter, double value)
{
	BitPos index = get_bit_pos(letter);
	if (index == BitPos::ERR)
		return InterError(InterError::INVALID_STATEMENT, std::string("invalid letter: ") + letter);

	if(flagValue.get((int)index))
		return InterError(InterError::DOUBLE_DEFINITION, std::string("duplicate letter: ") + letter);

	flagValue.set((int)index, true);
	this->value[int(index)] = value;

	return InterError();
}

//====================================================================================================
bool GCodeFrameParser::get_value(char letter, double &value)
{
	BitPos index = get_bit_pos(letter);
	if (index == BitPos::ERR)
		return false;

	if(!flagValue.get((int)index))
		return false;

	flagValue.set((int)index, false);
	value = this->value[int(index)];

	return true;
}

//====================================================================================================
bool GCodeFrameParser::have_value(char letter)
{
	BitPos index = get_bit_pos(letter);
	if (index == BitPos::ERR)
		return false;

	return flagValue.get((int)index);
}

//====================================================================================================
//очищает прочитанные данные фрейма
void GCodeFrameParser::reset()
{
	flagValue.reset();
	flagModal.reset();
	sendWait = false; //G4 readed
	plane = Plane::NONE;    //G17
	units = UnitSystem::NONE; //G20
	absoluteSet = false; //G53
	coordSystemNumber = -1;
	incremental = -1;
	motionMode = MotionMode::NONE;
	feedMode = FeedMode::NONE;
	cycle = CannedCycle::NONE;
	cycleLevel = CannedLevel::NONE;
}

//====================================================================================================
//формирует параметры перехода в новое состояние
InterError GCodeFrameParser::make_new_state(GCodeLexer& lexer)
{
	reset();

	for(const auto& code : lexer.codes)
	{
		int intValue = int(code.value);

		ModalGroup group = get_modal_group(code.letter, code.value);
		if ((int)group > 0)
		{
			if(flagModal.get((int)group))   //встретили два оператора из одной группы
			{
				lexer.position = code.position;
				return InterError(InterError::DOUBLE_DEFINITION, 
					std::string("conflict modal group for ") + code.letter + to_string(code.value));
			}
			flagModal.set((int)group, true);
		}

		switch(code.letter)
		{
			case 'G':
			{
				if (code.value == 94.1) {
					feedMode = FeedMode::THROTTLING;
					break;
				}

				if (code.value == 94.2) {
					feedMode = FeedMode::ADC;
					break;
				}

				if (code.value == 95.1) {
					feedMode = FeedMode::STABLE_REV;
					break;
				}

				switch (intValue)
				{
					case 0: motionMode = MotionMode::FAST; break;
					case 1: motionMode = MotionMode::LINEAR; break;
					case 2: motionMode = MotionMode::CW_ARC; break;
					case 3: motionMode = MotionMode::CCW_ARC; break;
					case 32: motionMode = MotionMode::LINEAR_SYNC; break;

					case 4: sendWait = true; break;
					case 17: plane = Plane::XY; break;
					case 18: plane = Plane::ZX; break;
					case 19: plane = Plane::YZ; break;

					case 20: units = UnitSystem::INCHES; break;
					case 21: units = UnitSystem::METRIC; break;

					case 53: absoluteSet = true; break;

					case 54: case 55: case 56: case 57: case 58:
						coordSystemNumber = intValue - 54; break;

					case 80: cycle = CannedCycle::RESET; break;
					case 81: cycle = CannedCycle::SINGLE_DRILL; break;
					case 82: cycle = CannedCycle::DRILL_AND_PAUSE; break;
					case 83: cycle = CannedCycle::DEEP_DRILL; break;

					case 90: incremental = 0; break;
					case 91: incremental = 1; break;

					case 94: feedMode = FeedMode::PER_MIN; break;
					case 95: feedMode = FeedMode::PER_REV; break;

					case 98: cycleLevel = CannedLevel::HIGH; break;
					case 99: cycleLevel = CannedLevel::LOW; break;

					default: return InterError(InterError::INVALID_STATEMENT, 
									std::string("unknown code: G") + to_string(intValue));
				}
				break;
			}

			case 'M': //пока у меня никакого охлаждения нет
			{
				switch (intValue)
				{
					default: break; //return InterError_INVALID_STATEMENT;
				}
				break;
			}

			case 'A':
			case 'B':
			case 'C':

			case 'X':
			case 'Y':
			case 'Z':

			case 'I':
			case 'J':
			case 'K':

			case 'P':
			case 'Q':
			case 'F':
			case 'S':
			case 'R':
			case 'D':
			case 'L':
				set_value(code.letter, code.value);
				break;

			case 'N':
				break;

			default: return InterError(InterError::INVALID_STATEMENT, std::string("invalid letter: ") + code.letter);
		}
	}

	return InterError();
}

//====================================================================================================
//возвращает модальную группу команды
ModalGroup GCodeFrameParser::get_modal_group(char letter, double value)
{
	int num = int(value);
	if(letter == 'G')
	{
		if (value == 94.1)
			return ModalGroup::FEED_MODE;

		if (value == 94.2)
			return ModalGroup::FEED_MODE;

		if (value == 95.1)
			return ModalGroup::FEED_MODE;

		switch(num)
		{
		case 0: case 1: case 2: case 3:
		case 32:
		case 80: case 81: case 82: case 83: case 84: case 85: case 86: case 87: case 88: case 89:
			return ModalGroup::MOVE;

		case 90: case 91:
			return ModalGroup::INCREMENTAL;

		case 20: case 21:
			return ModalGroup::UNITS;

		case 94: case 95:
			return ModalGroup::FEED_MODE;

		case 54: case 55: case 56: case 57: case 58:
			return ModalGroup::COORD_SYSTEM;

		case 43: case 44: case 49:
			return ModalGroup::TOOL_LENGTH_CORRECTION;

		case 40: case 41: case 42:
			return ModalGroup::TOOL_RADIUS_CORRECTION;

		case 98: case 99:
			return ModalGroup::CYCLE_RETURN;

		case 17: case 18: case 19:
			return ModalGroup::ACTIVE_PLANE;

		default:
			return ModalGroup::NONE;
		}
	}
	else if (letter == 'M')
	{
		switch (num)
		{
		case 0: case 1: case 2: case 30: case 60:
			return ModalGroup::STOP;

		case 6:
			return ModalGroup::TOOL_CHANGE;

		case 3: case 4: case 5:
			return ModalGroup::TURN_TOOL;

		case 7: case 8: case 9:
			return ModalGroup::GREASER;

		default:
			return ModalGroup::NONE;
		}
	}
	else
		return ModalGroup::NONE;
}
