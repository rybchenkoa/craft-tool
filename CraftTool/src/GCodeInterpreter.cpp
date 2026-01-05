#include "GCodeInterpreter.h"
#include "log.h"
#include "config_defines.h"

using namespace Interpreter;

#define MM_PER_INCHES 2.54
#define PI 3.14159265358979323846

//====================================================================================================
coord length(Coords from, Coords to)
{
    Coords delta;
    delta.x = from.x - to.x;
    delta.y = from.y - to.y;
    delta.z = from.z - to.z;
    return sqrtl(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
}

//====================================================================================================
inline double pow2(double x)
{
    return x*x;
}

//====================================================================================================
BitPos FrameParams::get_bit_pos(char letter)
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
InterError FrameParams::set_value(char letter, double value)
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
bool FrameParams::get_value(char letter, double &value)
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
bool FrameParams::have_value(char letter)
{
    BitPos index = get_bit_pos(letter);
	if (index == BitPos::ERR)
        return false;

	return flagValue.get((int)index);
}

//====================================================================================================
GCodeInterpreter::GCodeInterpreter(void)
{
    remoteDevice = nullptr;
}

//====================================================================================================
GCodeInterpreter::~GCodeInterpreter(void)
{
}

//====================================================================================================
InterError GCodeInterpreter::execute_frame(const char *frame)
{
    InterError state;

    state = reader.parse_codes(frame); //проверяем строку на валидность, читаем значения в массив
    if(state.code) return state;

    state = make_new_state(); //читаем все коды и по ним создаём команды изменения состояния
    if(state.code) return state;

    state = run_modal_groups(); //исполняем коды или пересылаем их устройству
    if(state.code) return state;

    return state;
}

//====================================================================================================
//читает данные из строки в массив
InterError Reader::parse_codes(const char *frame)
{
    codes.clear();
    position = 0;
    string = frame;
    state = InterError();

    GKey current;
    current.position = position;
    while(parse_code(current.letter, current.value))
    {
        codes.push_back(current);
        current.position = position;
    }

    return state;
}

//====================================================================================================
//очищает прочитанные данные фрейма
void FrameParams::reset()
{
    flagValue.reset();
    flagModal.reset();
    sendWait = false; //G4 readed
	plane = Plane::NONE;    //G17
    absoluteSet = false; //G53
	motionMode = MotionMode::NONE;
	feedMode = FeedMode::NONE;
	cycle = CannedCycle::NONE;
	cycleLevel = CannedLevel::NONE;
}

//====================================================================================================
//формирует параметры перехода в новое состояние
InterError GCodeInterpreter::make_new_state()
{
    readedFrame.reset();

    for(const auto& code : reader.codes)
    {
        int intValue = int(code.value);

        ModalGroup group = get_modal_group(code.letter, code.value);
		if ((int)group > 0)
        {
			if(readedFrame.flagModal.get((int)group))   //встретили два оператора из одной группы
            {
                reader.position = code.position;
                return InterError(InterError::DOUBLE_DEFINITION, 
					std::string("conflict modal group for ") + code.letter + to_string(code.value));
            }
			readedFrame.flagModal.set((int)group, true);
        }

        switch(code.letter)
        {
            case 'G':
            {
				if (code.value == 95.1) {
					readedFrame.feedMode = FeedMode::STABLE_REV;
					break;
				}

                switch (intValue)
                {
					case 0: readedFrame.motionMode = MotionMode::FAST; break;
					case 1: readedFrame.motionMode = MotionMode::LINEAR; break;
					case 2: readedFrame.motionMode = MotionMode::CW_ARC; break;
					case 3: readedFrame.motionMode = MotionMode::CCW_ARC; break;
					case 32: readedFrame.motionMode = MotionMode::LINEAR_SYNC; break;

                    case 4: readedFrame.sendWait = true; break;
					case 17: readedFrame.plane = Plane::XY; break;
					case 18: readedFrame.plane = Plane::ZX; break;
					case 19: readedFrame.plane = Plane::YZ; break;

					case 20: runner.units = UnitSystem::INCHES; break;
					case 21: runner.units = UnitSystem::METRIC; break;

                    case 53: readedFrame.absoluteSet = true; break;

                    case 54: case 55: case 56: case 57: case 58:
                        runner.coordSystemNumber = intValue - 54; break;

					case 80: readedFrame.cycle = CannedCycle::RESET; break;
					case 81: readedFrame.cycle = CannedCycle::SINGLE_DRILL; break;
					case 82: readedFrame.cycle = CannedCycle::DRILL_AND_PAUSE; break;
					case 83: readedFrame.cycle = CannedCycle::DEEP_DRILL; break;

                    case 90: runner.incremental = false; break;
                    case 91: runner.incremental = true; break;

					case 94: readedFrame.feedMode = FeedMode::PER_MIN; break;
					case 95: readedFrame.feedMode = FeedMode::PER_REV; break;

					case 98: readedFrame.cycleLevel = CannedLevel::HIGH; break;
					case 99: readedFrame.cycleLevel = CannedLevel::LOW; break;

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
                readedFrame.set_value(code.letter, code.value);
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
ModalGroup GCodeInterpreter::get_modal_group(char letter, double value)
{
    int num = int(value);
    if(letter == 'G')
    {
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

//====================================================================================================
//исполняет прочитанный фрейм в нужном порядке
InterError GCodeInterpreter::run_modal_groups()
{
    double value;
	InterError error;

	if (readedFrame.plane != Plane::NONE) { // задаём плоскость обработки
		runner.plane = readedFrame.plane;
	}

	error = run_feed_mode(); // режим контроля подачи (G94 и т.д.)
	if (error.code) return error;

	error = run_feed_rate(); // подача (F)
	if (error.code) return error;

    if(readedFrame.get_value('S', value)) //скорость вращения шпинделя
        if (!trajectory)
            remoteDevice->set_spindle_vel(value);

	update_motion_mode(); // обработка смены режима перемещения (G0, G1, G32...)
	error = update_cycle_params(); // обновление параметров постоянных циклов (G80...)
	if (error.code) return error;

	error = run_dwell(); // обработка паузы
	if (error.code) return error;

	if(runner.cycle != CannedCycle::NONE) //включен постоянный цикл
    {
		return run_modal_group_cycles();
    }
	else if(runner.motionMode == MotionMode::FAST ||
			runner.motionMode == MotionMode::LINEAR ||
			runner.motionMode == MotionMode::LINEAR_SYNC) //движение по прямой
	{
		return run_modal_group_linear();
	}
	else if(runner.motionMode == MotionMode::CW_ARC || runner.motionMode == MotionMode::CCW_ARC)
    {
		return run_modal_group_arc();
    }

    return InterError();
}

//====================================================================================================
// обновляет режим контроля подачи из текущего фрейма
InterError GCodeInterpreter::run_feed_mode()
{
	double value;

	switch (readedFrame.feedMode)
	{
		case FeedMode::PER_MIN:
			//TODO обработать P=0
			if (!trajectory) {
				remoteDevice->set_feed_normal();
			}
			runner.feedMode = readedFrame.feedMode;
			runner.feedModeRollback = readedFrame.feedMode;
			break;

		case FeedMode::PER_REV:
			// здесь только проверим, что подача будет задана, само задание дальше по коду
			if (!readedFrame.have_value('F'))
				return InterError(InterError::NO_VALUE, "expected F parameter");

			runner.feedMode = readedFrame.feedMode;
			runner.feedModeRollback = readedFrame.feedMode;
			break;

		case FeedMode::STABLE_REV:
			// здесь сразу прочитаем параметр, потому что команда не модальная
			if (!readedFrame.get_value('P', value))
				return InterError(InterError::NO_VALUE, "expected P parameter");

			if (!trajectory) {
				remoteDevice->set_feed_stable(value);
			}
			runner.feedStableFreq = value;
			// сбрасываем, так как подача на оборот не работает с ним параллельно
			runner.feedMode = FeedMode::PER_MIN;
			runner.feedModeRollback = readedFrame.feedMode;
			break;
	}

	return InterError();
}

//====================================================================================================
// задаёт подачу из текущего фрейма
InterError GCodeInterpreter::run_feed_rate()
{
	double value;

	if (readedFrame.get_value('F', value)) { // скорость подачи
		switch (runner.feedMode)
		{
			case FeedMode::PER_MIN:
				if (!trajectory)
					remoteDevice->set_feed(value / 60);
				runner.feed = value;
				break;

			case FeedMode::PER_REV:
				if (!trajectory)
					remoteDevice->set_feed_per_rev(value);
				runner.feedPerRev = value;
				break;
		}
	}

	return InterError();
}

//====================================================================================================
// обновляет режим перемещения из текущего фрейма
void GCodeInterpreter::update_motion_mode()
{
	MotionMode newMode = readedFrame.motionMode;

	// постоянные циклы тоже должны отключать режим синхронизации со шпинделем
	if (readedFrame.cycle != CannedCycle::NONE)
		newMode = MotionMode::LINEAR;

	if (newMode != MotionMode::NONE && newMode != runner.motionMode) {
		//G32 и режим перемещения, и способ задания подачи, поэтому тут особая обработка
		if (runner.motionMode == MotionMode::LINEAR_SYNC) {
			if (!trajectory) {
				switch (runner.feedModeRollback)
				{
					case FeedMode::PER_MIN:
						remoteDevice->set_feed_normal(); //выключаем синхронизацию со шпинделем
						break;

					case FeedMode::PER_REV:
						remoteDevice->set_feed_per_rev(runner.feedPerRev);
						break;

					case FeedMode::STABLE_REV:
						remoteDevice->set_feed_stable(runner.feedStableFreq);
						break;
				}
			}
		}
		runner.motionMode = newMode;
	}

	switch (newMode)
	{
		case MotionMode::NONE: break;

		case MotionMode::FAST:    set_move_mode(MoveMode_FAST); break;
		case MotionMode::LINEAR:  set_move_mode(MoveMode_LINEAR); break;
		case MotionMode::CCW_ARC: set_move_mode(MoveMode_LINEAR); break;
		case MotionMode::CW_ARC:  set_move_mode(MoveMode_LINEAR); break;
		case MotionMode::LINEAR_SYNC: set_move_mode(MoveMode_LINEAR); break;
	}
}

//====================================================================================================
// обновляет данные циклов из текущего фрейма
InterError GCodeInterpreter::update_cycle_params()
{
	switch(readedFrame.cycle)
	{
		case CannedCycle::NONE: break;

		case CannedCycle::RESET:
			runner.cycle = CannedCycle::NONE; break;

		case CannedCycle::SINGLE_DRILL:
		case CannedCycle::DRILL_AND_PAUSE:
		case CannedCycle::DEEP_DRILL:
		{
			if(runner.cycle != CannedCycle::NONE)
				return InterError(InterError::DOUBLE_DEFINITION, "repeat canned cycle call");

			runner.cycle = readedFrame.cycle;

			if(!readedFrame.have_value('R'))
				return InterError(InterError::NO_VALUE, "expected R parameter");

			if(!readedFrame.have_value('Z'))
				return InterError(InterError::NO_VALUE, "expected Z parameter");

			if(!readedFrame.have_value('P') && runner.cycle == CannedCycle::DRILL_AND_PAUSE)
				return InterError(InterError::NO_VALUE, "expected P parameter");

			if(!readedFrame.have_value('Q') && runner.cycle == CannedCycle::DEEP_DRILL)
				return InterError(InterError::NO_VALUE, "expected Q parameter");

			runner.cycleHiLevel = runner.position.z;

			double value;

			get_readed_coord('R', value);
			if(runner.incremental)
				value += runner.cycleHiLevel;
			runner.cycleLowLevel = value;

			get_readed_coord('Z', value);
			//где-то это глубина от R, где-то конечная координата, можно флагом выбирать поведение
			if (runner.incremental || runner.cycle == CannedCycle::SINGLE_DRILL && runner.cycle81Incremental)
				value += runner.cycleLowLevel;
			runner.cycleDeepLevel = value;

			if(runner.cycle == CannedCycle::DEEP_DRILL)
				get_readed_coord('Q', runner.cycleStep);

			if(runner.cycle == CannedCycle::DRILL_AND_PAUSE ||
				runner.cycle == CannedCycle::DEEP_DRILL)
			{
				readedFrame.get_value('P', value);
				runner.cycleWait = value;
			}

			break;
		}
	}

	switch(readedFrame.cycleLevel)
	{
		case CannedLevel::NONE: break;
		case CannedLevel::HIGH: runner.cycleUseLowLevel = false; break;
		case CannedLevel::LOW: runner.cycleUseLowLevel = true; break;
	}

	return InterError();
}

//====================================================================================================
// исполняет задержку
InterError GCodeInterpreter::run_dwell()
{
	if (readedFrame.sendWait)
	{
		double value;
		if (!readedFrame.get_value('P', value))
			return InterError(InterError::NO_VALUE, "expected P parameter");

		if (value < 0)
			return InterError(InterError::WRONG_VALUE, std::string("invalid P parameter") + to_string(value));

		if (!trajectory)
			remoteDevice->wait(value);
	}

	return InterError();
}

//====================================================================================================
// исполняет линейное движение текущего фрейма
InterError GCodeInterpreter::run_modal_group_linear()
{
	Coords pos = runner.position;
	bool coordsSet = false;
	if(readedFrame.absoluteSet)
	{
		get_readed_coord('X', pos.x);
		get_readed_coord('Y', pos.y);
		get_readed_coord('Z', pos.z);
		get_readed_coord('A', pos.a);
		get_readed_coord('B', pos.b);
		coordsSet = true;
	}
	else if (get_new_position(pos)) {
		coordsSet = true;
	}

	if(runner.motionMode == MotionMode::LINEAR_SYNC) { //TODO проверить выключение синхронизации
		auto error = run_linear_sync(pos);
		if (error.code != error.ALL_OK)
			return error;
	}

	if (coordsSet) {
		runner.position = pos;
		move_to(pos);
	}

	return InterError();
}

//====================================================================================================
//исполняет линейное движение текущего фрейма с синхронизацией со шпинделем
InterError GCodeInterpreter::run_linear_sync(Coords& pos)
{
	//дополнительная инициализация на старте
	if(readedFrame.motionMode == MotionMode::LINEAR_SYNC) {
		runner.spindleAngle = 0; //прочитаем дальше, если будет
		if(!readedFrame.have_value('K')) //шаг резьбы обязательно задаём на старте, дальше опционально
			return InterError(InterError::NO_VALUE, "expected K parameter");
		//выбираем самую длинную ось в качестве синхронной
		int index = 0;
		coord delta = abs(pos.r[0] - runner.position.r[0]);
		for (int i = 1; i < NUM_COORDS; ++i) {
			coord newDelta = abs(pos.r[i] - runner.position.r[i]);
			if (newDelta > delta) {
				index = i;
				delta = newDelta;
			}
		}
		runner.threadIndex = index;
	}

	if(readedFrame.get_value('Q', runner.spindleAngle)) //угол захода при нарезании резьбы, в градусах
		runner.spindleAngle = runner.spindleAngle / 360;
	readedFrame.get_value('K', runner.threadPitch); //шаг витка, в оборотах

	//координата места, где угол нулевой
	coord base = runner.position.r[runner.threadIndex] - runner.threadPitch * runner.spindleAngle;
	if (!trajectory)
		remoteDevice->set_feed_sync(runner.threadPitch, base, runner.threadIndex);

	//после отсылки команды пересчитываем угол
	coord delta = abs(pos.r[runner.threadIndex] - runner.position.r[runner.threadIndex]);
	runner.spindleAngle += delta / runner.threadPitch;

	return InterError();
}

//====================================================================================================
//исполняет круговое движение текущего фрейма
InterError GCodeInterpreter::run_modal_group_arc()
{
	if(readedFrame.absoluteSet)
		return InterError(InterError::WRONG_VALUE, "use absolute system G53 with arc");

	int ix, iy, iz;
	switch(runner.plane)
	{
		case Plane::NONE: //assume(0);
		case Plane::XY: ix = 0; iy = 1; iz = 2; break;
		case Plane::YZ: ix = 1; iy = 2; iz = 0; break;
		case Plane::ZX: ix = 2; iy = 0; iz = 1; break;
	}

	Coords centerPos;

	if(readedFrame.have_value('I') ||  //если задан центр круга
		readedFrame.have_value('J') ||
		readedFrame.have_value('K'))
	{
		if(readedFrame.have_value('R'))
			return InterError(InterError::WRONG_VALUE, "conflict parameter R with I,J,K offset");

		//изначально координаты нулевые centerPos.x = centerPos.y = centerPos.z = 0;
		get_readed_coord('I', centerPos.x); //читаем центр круга
		get_readed_coord('J', centerPos.y);
		get_readed_coord('K', centerPos.z);

		centerPos += runner.position;

		Coords pos = runner.position;            //читаем, докуда двигаться
		get_new_position(pos);

		coord pitch = centerPos.r[iz] - runner.position.r[iz];//шаг винта
		centerPos.r[iz] = pos.r[iz];      //третий параметр означает нечто другое
		Coords planeCenter = centerPos;
		planeCenter.r[iz] = runner.position.r[iz];
		double radius = length(runner.position, planeCenter);

		auto len = length(pos, centerPos);
		if(fabs(radius - len) > remoteDevice->get_min_step(ix, iy)*2)//растяжение пока не поддерживается
			return InterError(InterError::WRONG_VALUE, "arc endpoint unreachable, start and endpoint have difference radius");

		double angleStart = atan2(runner.position.r[iy] - planeCenter.r[iy], runner.position.r[ix] - planeCenter.r[ix]);
		coord height = pos.r[iz] - runner.position.r[iz]; //длина винта

		if(is_screw(centerPos) && pitch != 0) //винтовая интерполяция
		{
			if(pitch < 0.0)
				return InterError(InterError::WRONG_VALUE, "screw with negative pitch");
			else
			{
				double countTurns = height / pitch;
				double angleMax = fabs(countTurns * 2 * PI);
				double zScale = height / angleMax;

				draw_screw(planeCenter, radius, 1.0, angleStart, angleMax, zScale, ix, iy, iz);
			}
			if(length(runner.position, pos) > remoteDevice->get_min_step(ix, iy)*2)
				return InterError(InterError::WRONG_VALUE, "unexpected fail to reach arc end point");

			runner.position = pos;
		}
		else
		{
			double angleMax = atan2(pos.r[iy] - planeCenter.r[iy], pos.r[ix] - planeCenter.r[ix]);
			angleMax -= angleStart;
			if(runner.motionMode == MotionMode::CCW_ARC)
			{
				if(angleMax <= 0)
					angleMax += 2 * PI;
			}
			else
			{
				angleMax *= -1;
				if(angleMax <= 0)
					angleMax += 2 * PI;
			}

			double zScale = height / angleMax;

			draw_screw(planeCenter, radius, 1.0, angleStart, angleMax, zScale, ix, iy, iz);

			if(length(runner.position, pos) > remoteDevice->get_min_step(ix, iy)*2)
				return InterError(InterError::WRONG_VALUE, "unexpected fail to reach arc end point");

			runner.position = pos;
		}
	}
	else if(readedFrame.have_value('R'))
	{
		coord radius;
		get_readed_coord('R', radius);

		Coords pos = runner.position;            //читаем, докуда двигаться
		get_new_position(pos);

		double distance = length(runner.position, pos);
		if(fabs(distance / radius) < 0.01)       //плохо вычисляемая окружность
			return InterError(InterError::WRONG_VALUE, "poorly calculated arc");

		if(distance > fabs(radius * 2))              //неверный радиус
			return InterError(InterError::WRONG_VALUE, "arc endpoint unreachable");

		if(is_screw(pos)) //винтовая интерполяция пока не поддерживается
			return InterError(InterError::WRONG_VALUE, "screw with R parameter not supported"); //TODO хорошо бы поддержать

		//используется теорема Пифагора
		Coords toCenter; //находим направление от центра отрезка к центру окружности
		double length = sqrt(std::max(0.0, radius * radius - distance * distance / 4)); //длина перпендикуляра
		if((radius < 0) == (runner.motionMode == MotionMode::CCW_ARC))
			length *= -1;
		for(int i = 0; i < NUM_COORDS; ++i)
			toCenter.r[i] = (runner.position.r[i] - pos.r[i]) * length / distance;

		std::swap(toCenter.r[ix], toCenter.r[iy]);
		toCenter.r[iy] = -toCenter.r[iy];

		for(int i = 0; i < NUM_COORDS; ++i)
			centerPos.r[i] = (runner.position.r[i] + pos.r[i]) / 2 + toCenter.r[i];

		double angleStart = atan2(runner.position.r[iy] - centerPos.r[iy], runner.position.r[ix] - centerPos.r[ix]);
		double angleMax = atan2(pos.r[iy] - centerPos.r[iy], pos.r[ix] - centerPos.r[ix]);
		angleMax -= angleStart;
		if(runner.motionMode == MotionMode::CCW_ARC)
		{
			if(angleMax <= 0)
				angleMax += 2 * PI;
		}
		else
		{
			angleMax *= -1;
			if(angleMax <= 0)
				angleMax += 2 * PI;
		}

		draw_screw(centerPos, fabs(radius), 1.0, angleStart, angleMax, 0, ix, iy, iz);

		runner.position = pos;
	}

	return InterError();
}

//====================================================================================================
//исполняет цикл текущего фрейма
InterError GCodeInterpreter::run_modal_group_cycles()
{
	if(readedFrame.have_value('Z'))    //в нём нельзя двигаться по Z
		return InterError(InterError::WRONG_VALUE, "unexpected Z parameter");

	Coords pos;
	auto moveTo = [&](Coords pos)
	{
		to_global(pos);
		move_to(pos);
	};

	if(get_new_position(pos))
	{
		to_local(pos);
		if(runner.cycleUseLowLevel)
			pos.z = runner.cycleLowLevel;
		else
			pos.z = runner.cycleHiLevel;

		set_move_mode(MoveMode_FAST);
		moveTo(pos);    //двигаемся к следующему отверстию

		auto pos2 = pos;

		if(!runner.cycleUseLowLevel)
		{
			pos2.z = runner.cycleLowLevel;
			moveTo(pos2);  //двигаемся к безопасной плоскости
		}

		switch(runner.cycle)
		{
			case CannedCycle::NONE: //assume(0);
			case CannedCycle::RESET: //assume(0);
			case CannedCycle::SINGLE_DRILL:
			{
				set_move_mode(MoveMode_LINEAR);
				pos2.z = runner.cycleDeepLevel;
				moveTo(pos2);
				set_move_mode(MoveMode_FAST);
				moveTo(pos);
				break;
			}
			case CannedCycle::DRILL_AND_PAUSE:
			{
				set_move_mode(MoveMode_LINEAR);
				pos2.z = runner.cycleDeepLevel;
				moveTo(pos2);
				if (!trajectory)
					remoteDevice->wait(runner.cycleWait);
				set_move_mode(MoveMode_FAST);

				moveTo(pos);
				break;
			}
			case CannedCycle::DEEP_DRILL:
			{
				coord curZ = pos.z - runner.cycleStep;
				while(curZ >= runner.cycleDeepLevel)
				{
					set_move_mode(MoveMode_FAST);
					pos2.z = curZ + runner.cycleStep;
					moveTo(pos2);

					set_move_mode(MoveMode_LINEAR);
					pos2.z = curZ;
					moveTo(pos2);

					if(runner.cycleWait != 0.0)
						if (!trajectory)
							remoteDevice->wait(runner.cycleWait);

					set_move_mode(MoveMode_FAST);
					pos2.z = runner.cycleLowLevel;
					moveTo(pos2);

					if(curZ == runner.cycleDeepLevel)
						break;

					curZ -= runner.cycleStep;
					if(curZ < runner.cycleDeepLevel)
						curZ = runner.cycleDeepLevel;
				}

				set_move_mode(MoveMode_FAST);
				moveTo(pos);

				break;
			}
		}

		to_global(pos);
		runner.position = pos;
	}

	return InterError();
}

//====================================================================================================
void GCodeInterpreter::set_move_mode(MoveMode mode)
{
    runner.deviceMoveMode = mode;
    if (!trajectory)
        remoteDevice->set_move_mode(mode);
}

//====================================================================================================
void GCodeInterpreter::move_to(Coords position)
{
    if (!trajectory)
        remoteDevice->set_position(position);
    else
    {
        TrajectoryPoint point;
        point.position = position;
        point.isFast = (runner.deviceMoveMode == MoveMode_FAST);
        trajectory->push_back(point);
    }
}

//====================================================================================================
bool GCodeInterpreter::is_screw(Coords center)
{
	switch (runner.plane)
	{
		case Plane::XY: return center.z != runner.position.z;
		case Plane::ZX: return center.y != runner.position.y;
		case Plane::YZ: return center.x != runner.position.x;
		default: return false;
	}
}

//====================================================================================================
//рисует винтовую линию или просто круг
void GCodeInterpreter::draw_screw(Coords center, double radius, double ellipseCoef,
                double angleStart, double angleMax, double angleToHeight,
                int ix, int iy, int iz)
{
    double accuracy = remoteDevice->get_min_step(ix, iy);
    //шаг угла выбираем таким, чтобы точность была в пределах одного шага
    //ряд Тейлора для синуса sin(x) = x +...
    //при повороте от оси Х на заданный угол ошибка по второй оси должна быть равна погрешности
    //  r - r*cos(a) = accuracy
    //  1 - cos(a) = acc/r
    //  cos(a) = sqrt(1-sin^2(a)) ~= sqrt(1-a^2)
    //  1 - sqrt(1 - step^2) = acc / r;
    //  1 - step^2 = (1 - acc/r)^2
    //  step = sqrt(1-(1-acc/r)^2)
    double step = sqrt(1-pow2(1-accuracy/radius));

    double aScale = 1;
	if(runner.motionMode == MotionMode::CW_ARC)
        aScale = -1;

    Coords curPos = center;
	auto moveTo = [&] (double angle)
	{
		curPos.r[ix] = center.r[ix] + radius * cos(angleStart + angle * aScale);
        curPos.r[iy] = center.r[iy] + radius * sin(angleStart + angle * aScale) * ellipseCoef;
        curPos.r[iz] = center.r[iz] + angle * angleToHeight;
        move_to(curPos);
	};

    double angle = 0;
    for(; angle < angleMax; angle += step)
		moveTo(angle);
    moveTo(angleMax);
    runner.position = curPos;
}

//====================================================================================================
//чтение новых координат с учётом модальных кодов
bool GCodeInterpreter::get_new_position(Coords &pos)
{
    if(readedFrame.have_value('X') ||
       readedFrame.have_value('Y') ||
       readedFrame.have_value('Z') ||
	   readedFrame.have_value('A') ||
	   readedFrame.have_value('B')
	   )
    {
        if(runner.incremental)
        {
            pos = Coords();
        }
        else
        {
            pos = runner.position;
            to_local(pos);
        }

        //координаты указаны в локальной системе
        get_readed_coord('X', pos.x);
        get_readed_coord('Y', pos.y);
        get_readed_coord('Z', pos.z);
		get_readed_coord('A', pos.a);
		get_readed_coord('B', pos.b);

        if(runner.incremental)
        {
            pos += runner.position;
        }
        else
            to_global(pos);

        return true;
    }

    return false;
}

//====================================================================================================
//сдвиг в глобальные координаты
void GCodeInterpreter::to_global(Coords &coords)
{
    if(runner.coordSystemNumber == -1)
        return;

    coords += runner.csd[runner.coordSystemNumber].pos0;
}

//====================================================================================================
//получение локальных координат из глобальных
void GCodeInterpreter::to_local(Coords &coords)
{
    if(runner.coordSystemNumber == -1)
        return;

    coords -= runner.csd[runner.coordSystemNumber].pos0;
}

//====================================================================================================
//преобразования локальных координат
void GCodeInterpreter::local_deform(Coords &coords)
{
    Q_UNUSED(coords)
}

//====================================================================================================
//перевод в мм
coord GCodeInterpreter::to_mm(coord value)
{
    if(runner.units == UnitSystem::INCHES)
        value *= MM_PER_INCHES;
    return value;
}

//====================================================================================================
Coords GCodeInterpreter::to_mm(Coords value)
{
    for(int i = 0; i < NUM_COORDS; ++i)
        value.r[i] = to_mm(value.r[i]);
    return value;
}

//====================================================================================================
double move_length(double t, double v, double a)
{
    double t1 = v/a;
    if (t1 > t)
        return a*t*t*0.5;
    else
        return a*t1*t1*0.5 + v*(t-t1);
}

//====================================================================================================
void GCodeInterpreter::move(int coordNumber, coord add, bool fast)
{
    //во время исполнения ничего не двигаем
    if(remoteDevice->queue_size() > 0)
        return;

	runner.motionMode = MotionMode::FAST;
    remoteDevice->set_move_mode(MoveMode_FAST);

    //если только подключились к устройству, то координаты могут быть очень разными
    if(!coordsInited)
    {
        runner.position = *remoteDevice->get_current_coords();
        coordsInited = true;
    }

    double delta = abs(runner.position.r[coordNumber] - remoteDevice->get_current_coords()->r[coordNumber]);

    if(delta > 10) //защита от багов
        return;

    if (!fast)
      runner.position.r[coordNumber] += add;
    else
    {
      const double LATENCY = 0.3; //0.3 секунд на остановку после отпускания кнопки
      double vel = remoteDevice->get_max_velocity(coordNumber);
      double acc = remoteDevice->get_max_acceleration(coordNumber);
      double maxLen = move_length(LATENCY, vel, acc);

      if (maxLen > 10) //защита от багов
        maxLen = 10;

      double accWall = std::max(delta, abs(add)) * 5;
      if (maxLen > accWall) //защита от слишком быстрого разгона
        maxLen = accWall;

      int intDelta = abs((maxLen - delta) / add);
      runner.position.r[coordNumber] += add * intDelta;
    }
    remoteDevice->set_position(runner.position);
}

//====================================================================================================
bool GCodeInterpreter::get_readed_coord(char letter, coord &value)
{
    if(readedFrame.get_value(letter, value))
    {
        value = to_mm(value);
        return true;
    }
    return false;
}

//====================================================================================================
//читает следующий код
bool Reader::parse_code(char &letter, double &value) 
{
    find_significal_symbol();
    if (string[position] == 0)
        return false;

    letter = string[position];

    if(letter == '%')
        return false;

    if(letter >= 'a' && letter <= 'z')
        letter += 'A' - 'a';

    if(letter < 'A' || letter > 'Z')
    {
        state = InterError(InterError::WRONG_LETTER, std::string("wrong letter: ") + letter);
        return false;
    }

    position++;

    find_significal_symbol();
    if (!parse_value(value))
    {
        state = InterError(InterError::WRONG_VALUE, std::string("cant parse value"));
        return false;
    };

    return true;
}

//====================================================================================================
//пропускает пробелы
void Reader::accept_whitespace()
{
    while (string[position] == ' ' || string[position] == '\t') position++;
}

//====================================================================================================
//доходит до следующего кода
void Reader::find_significal_symbol()
{
    while(string[position] != 0)
    {
        accept_whitespace();
        if(string[position] == '(')
        {
            while(string[position] != ')' && string[position] != 0) position++;
            if(string[position] == ')') position++;
        }
        else
            break;
    }
}

//====================================================================================================
//читает число
bool Reader::parse_value(double &dst)
{
    const char *cursor = string + position;

    double value = 0;
    int sign = 1;       // +-
    int numDigits = 0;  //сколько цифр прочитано
    int maxDigits = 20; //сколько всего можно
    double denominator = 1;//на сколько поделить прочитанное

    if (*cursor == '-')
        sign = -1;
    else if (*cursor == '+')
        sign = 1;
    else if(*cursor >= '0' && *cursor <= '9')
    {
        value = *cursor - '0';
        numDigits++;
    }
    else if(*cursor != '.')
        return false;

    if(*cursor != '.')
        ++cursor;

    while(*cursor >= '0' && *cursor <= '9' && ++numDigits <= maxDigits)
        value = value*10 + (*(cursor++) - '0');

    if(numDigits > maxDigits)
    {
        position = cursor - string;
        return false;
    }

    if(*cursor == '.')
    {
        ++cursor;
        while(*cursor >= '0' && *cursor <= '9' && ++numDigits <= maxDigits)
        {
            value = value*10 + (*(cursor++) - '0');
            denominator *= 10;
        }

        if(numDigits > maxDigits)
        {
            position = cursor - string;
            return false;
        }

        value /= denominator;
    }

    dst = value * sign;
    position = cursor - string;
    return true;
}

//====================================================================================================
//читает строки в список
bool GCodeInterpreter::read_file(const char *name)
{
    std::ifstream file(name); //открываем файл

    inputFile.clear();
    while(file) //пока он не закончился
    {
        std::string str;
        std::getline(file, str); //читаем строку
        inputFile.push_back(str); //добавляем в конец списка
    }

    return true;
}

//====================================================================================================
//читает строки в список
void GCodeInterpreter::execute_file(Trajectory *trajectory)
{
    this->trajectory = trajectory;
    Runner runnerDump;
    if (trajectory)
        runnerDump = runner;
    int lineNumber = 0;
    for(const auto& line : inputFile)
    {
        if (!trajectory)
            remoteDevice->set_current_line(lineNumber);
        auto result = execute_frame(line.c_str());
        if(result.code)
            log_warning("[E] line %d, %s\n", lineNumber + 1, result.description.c_str());
        ++lineNumber;
    };
    if (trajectory)
        runner = runnerDump;
}

//====================================================================================================
//читает строки в список
void GCodeInterpreter::execute_line(std::string line)
{
	this->trajectory = nullptr;
    remoteDevice->set_current_line(0);
    auto result = execute_frame(line.c_str());
    if(result.code)
        log_warning("execute_frame error %s\n", result.description.c_str());
}

//====================================================================================================
void GCodeInterpreter::init()
{
    //reader.position = {0.0f, 0.0f, 0.0f};
    reader.state = InterError();

    runner.coordSystemNumber = 0;
    runner.cutterLength = 0;
    runner.cutterRadius = 0;
	runner.feedMode = FeedMode::PER_MIN;
	runner.feedModeRollback = FeedMode::PER_MIN;
    runner.feed = 600; // сейчас это значение справочное
	runner.feedPerRev = 0.05;
	runner.feedStableFreq = 10;
	runner.spindleAngle = 0;
	runner.threadPitch = 0;
	runner.threadIndex = 0;
    runner.incremental = false;
	runner.motionMode = MotionMode::FAST;
    runner.deviceMoveMode = MoveMode_FAST;
	runner.cycle = CannedCycle::NONE;
    runner.cycle81Incremental = g_config->get_int_def(CFG_G81_INCREMENTAL, 0);
	runner.plane = Plane::XY;
    runner.position = Coords();
    runner.units = UnitSystem::METRIC;
    memset(&runner.csd, 0, sizeof(runner.csd));
    coordsInited = false;
}

//====================================================================================================
std::vector<std::string> GCodeInterpreter::get_active_codes()
{
	std::vector<std::string> result;

	int plane = (int)runner.plane - (int)Plane::XY; // G17
	int cycle = (int)runner.cycle - (int)CannedCycle::SINGLE_DRILL; // G81

	if (runner.cycle != CannedCycle::NONE) {
		result.push_back("G" + std::to_string(81 + cycle));
	}
	else {
		switch (runner.motionMode) {
		case MotionMode::FAST: result.push_back("G0"); break;
		case MotionMode::LINEAR: result.push_back("G1"); break;
		case MotionMode::CW_ARC: result.push_back("G2"); break;
		case MotionMode::CCW_ARC: result.push_back("G3"); break;
		case MotionMode::LINEAR_SYNC: result.push_back("G32"); break;
		}
	}

	result.push_back("F" + (runner.feed == int(runner.feed) ?
		std::to_string(int(runner.feed)) :
		std::to_string(runner.feed)));

	result.push_back("G" + std::to_string(17 + plane));
	result.push_back(runner.units == UnitSystem::INCHES ? "G20" : "G21");
	result.push_back("G" + std::to_string(54 + runner.coordSystemNumber));
	result.push_back(runner.incremental ? "G91" : "G90");
	result.push_back(runner.cycleUseLowLevel ? "G99" : "G98");

	return result;
}

