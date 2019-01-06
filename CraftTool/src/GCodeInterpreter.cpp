#include <math.h>
#include "GCodeInterpreter.h"
#include "log.h"

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
        case 'X': return BitPos_X;
        case 'Y': return BitPos_Y;
        case 'Z': return BitPos_Z;

        case 'I': return BitPos_I;
        case 'J': return BitPos_J;
        case 'K': return BitPos_K;

        case 'F': return BitPos_F;
        case 'P': return BitPos_P;
        case 'Q': return BitPos_Q;
        case 'S': return BitPos_S;
        case 'R': return BitPos_R;
        case 'D': return BitPos_D;
        case 'L': return BitPos_L;

        default: return BitPos_ERR;
    }
}

//====================================================================================================
InterError FrameParams::set_value(char letter, double value)
{
    BitPos index = get_bit_pos(letter);
    if (index == BitPos_ERR)
        return InterError(InterError::INVALID_STATEMENT, std::string("invalid letter: ") + letter);

    if(flagValue.get(index))
        return InterError(InterError::DOUBLE_DEFINITION, std::string("duplicate letter: ") + letter);

    flagValue.set(index, true);
    this->value[int(index)] = value;

    return InterError();
}

//====================================================================================================
bool FrameParams::get_value(char letter, double &value)
{
    BitPos index = get_bit_pos(letter);
    if (index == BitPos_ERR)
        return false;

    if(!flagValue.get(index))
        return false;

    flagValue.set(index, false);
    value = this->value[int(index)];

    return true;
}

//====================================================================================================
bool FrameParams::have_value(char letter)
{
    BitPos index = get_bit_pos(letter);
    if (index == BitPos_ERR)
        return false;

    return flagValue.get(index);
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
    //char letter;
    //double value;

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
    plane = Plane_NONE;    //G17
    absoluteSet = false; //G53
    motionMode = MotionMode_NONE;
    cycle = CannedCycle_NONE;
    cycleLevel = CannedLevel_NONE;
}

//====================================================================================================
//формирует параметры перехода в новое состояние
InterError GCodeInterpreter::make_new_state()
{
    readedFrame.reset();

    for(auto iter = reader.codes.begin(); iter != reader.codes.end(); ++iter)
    {
        int intValue = int(iter->value);

        ModalGroup group = get_modal_group(iter->letter, iter->value);
        if (group > 0)
        {
            if(readedFrame.flagModal.get(group))   //встретили два оператора из одной группы
            {
                reader.position = iter->position;
                return InterError(InterError::DOUBLE_DEFINITION, 
					std::string("conflict modal group for ") + iter->letter + to_string(iter->value));
            }
            readedFrame.flagModal.set(group, true);
        }

        switch(iter->letter)
        {
            case 'G':
            {
                switch (intValue)
                {
                    case 0: readedFrame.motionMode = MotionMode_FAST; break;
                    case 1: readedFrame.motionMode = MotionMode_LINEAR; break;
                    case 2: readedFrame.motionMode = MotionMode_CW_ARC; break;
                    case 3: readedFrame.motionMode = MotionMode_CCW_ARC; break;

                    case 4: readedFrame.sendWait = true; break;
                    case 17: readedFrame.plane = Plane_XY; break;
                    case 18: readedFrame.plane = Plane_ZX; break;
                    case 19: readedFrame.plane = Plane_YZ; break;

                    case 20: runner.units = UnitSystem_INCHES; break;
                    case 21: runner.units = UnitSystem_METRIC; break;

                    case 53: readedFrame.absoluteSet = true; break;

                    case 54: case 55: case 56: case 57: case 58:
                        runner.coordSystemNumber = intValue - 54; break;

                    case 80: readedFrame.cycle = CannedCycle_RESET; break;
                    case 81: readedFrame.cycle = CannedCycle_SINGLE_DRILL; break;
                    case 82: readedFrame.cycle = CannedCycle_DRILL_AND_PAUSE; break;
                    case 83: readedFrame.cycle = CannedCycle_DEEP_DRILL; break;

                    case 90: runner.incremental = false; break;
                    case 91: runner.incremental = true; break;

                    case 98: readedFrame.cycleLevel = CannedLevel_HIGH; break;
                    case 99: readedFrame.cycleLevel = CannedLevel_LOW; break;

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
                readedFrame.set_value(iter->letter, iter->value);
                break;

            case 'N':
                break;

			default: return InterError(InterError::INVALID_STATEMENT, std::string("invalid letter: ") + iter->letter);
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
        switch(num)
        {
        case 0: case 1: case 2: case 3:
        case 80: case 81: case 82: case 83: case 84: case 85: case 86: case 87: case 88: case 89:
            return ModalGroup_MOVE;

        case 90: case 91:
            return ModalGroup_INCREMENTAL;

        case 20: case 21:
            return ModalGroup_UNITS;

        case 54: case 55: case 56: case 57: case 58:
            return ModalGroup_COORD_SYSTEM;

        case 43: case 44: case 49:
            return ModalGroup_TOOL_LENGTH_CORRECTION;

        case 40: case 41: case 42:
            return ModalGroup_TOOL_RADIUS_CORRECTION;

        case 98: case 99:
            return ModalGroup_CYCLE_RETURN;

        case 17: case 18: case 19:
            return ModalGroup_ACTIVE_PLANE;

        default:
            return ModalGroup_NONE;
        }
    }
    else if (letter == 'M')
    {
        switch (num)
        {
        case 0: case 1: case 2: case 30: case 60:
            return ModalGroup_STOP;

        case 6:
            return ModalGroup_TOOL_CHANGE;

        case 3: case 4: case 5:
            return ModalGroup_TURN_TOOL;

        case 7: case 8: case 9:
            return ModalGroup_GREASER;

        default:
            return ModalGroup_NONE;
        }
    }
    else
        return ModalGroup_NONE;
}

//====================================================================================================
//исполняет прочитанный фрейм в нужном порядке
InterError GCodeInterpreter::run_modal_groups()
{
    double value;

    switch (readedFrame.plane) //задаём плоскость обработки
    {
        case Plane_NONE: break;

        case Plane_XY:
            runner.plane = Plane_XY;
            break;
        case Plane_ZX:
            runner.plane = Plane_ZX;
            break;
        case Plane_YZ:
            runner.plane = Plane_YZ;
            break;

		default: return InterError(InterError::WRONG_PLANE, 
					 std::string("internal error, invalid plane ") + to_string(readedFrame.plane));
    }

    //if(readedFrame.get_value('S', value)) //скорость вращения шпинделя
    //    ;

    if(readedFrame.get_value('F', value)) //скорость подачи
        if (!trajectory)
            remoteDevice->set_feed(value / 60);

    switch (readedFrame.motionMode)
    {
        case MotionMode_NONE: break;

        case MotionMode_CCW_ARC:
        case MotionMode_CW_ARC:
        case MotionMode_FAST:
        case MotionMode_LINEAR:
            runner.motionMode = readedFrame.motionMode;
            break;
    }

    switch (readedFrame.motionMode)
    {
        case MotionMode_NONE: break;

        case MotionMode_FAST:    set_move_mode(MoveMode_FAST); break;
        case MotionMode_LINEAR:  set_move_mode(MoveMode_LINEAR); break;
        case MotionMode_CCW_ARC: set_move_mode(MoveMode_LINEAR); break;
        case MotionMode_CW_ARC:  set_move_mode(MoveMode_LINEAR); break;
    }

    switch(readedFrame.cycle)
    {
        case CannedCycle_NONE: break;

        case CannedCycle_RESET:
            runner.cycle = CannedCycle_NONE; break;

        case CannedCycle_SINGLE_DRILL:
        case CannedCycle_DRILL_AND_PAUSE:
        case CannedCycle_DEEP_DRILL:
        {
            if(runner.cycle != CannedCycle_NONE)
                return InterError(InterError::DOUBLE_DEFINITION, "repeat canned cycle call");

            runner.cycle = readedFrame.cycle;

            if(!readedFrame.have_value('R'))
                return InterError(InterError::NO_VALUE, "expected R parameter");

            if(!readedFrame.have_value('Z'))
                return InterError(InterError::NO_VALUE, "expected Z parameter");

            if(!readedFrame.have_value('P') && runner.cycle == CannedCycle_DRILL_AND_PAUSE)
                return InterError(InterError::NO_VALUE, "expected P parameter");

            if(!readedFrame.have_value('Q') && runner.cycle == CannedCycle_DEEP_DRILL)
                return InterError(InterError::NO_VALUE, "expected Q parameter");

            runner.cycleHiLevel = runner.position.z;

            double value;

            get_readed_coord('R', value);
            if(runner.incremental)
                value += runner.cycleHiLevel;
            runner.cycleLowLevel = value;

            get_readed_coord('Z', value);
            if(runner.incremental)
                value += runner.cycleLowLevel;
            runner.cycleDeepLevel = value;

            if(runner.cycle == CannedCycle_DEEP_DRILL)
                get_readed_coord('Q', runner.cycleStep);

            if(runner.cycle == CannedCycle_DRILL_AND_PAUSE ||
               runner.cycle == CannedCycle_DEEP_DRILL)
            {
                readedFrame.get_value('P', value);
                runner.cycleWait = value;
            }

            break;
        }
    }

    switch(readedFrame.cycleLevel)
    {
        case CannedLevel_NONE: break;
        case CannedLevel_HIGH: runner.cycleUseLowLevel = false; break;
        case CannedLevel_LOW: runner.cycleUseLowLevel = true; break;
    }

    if(readedFrame.sendWait)
    {
        if(!readedFrame.get_value('P', value))
            return InterError(InterError::NO_VALUE, "expected P parameter");

        if(value < 0)
            return InterError(InterError::WRONG_VALUE, std::string("invalid P parameter") + to_string(value));

        if (!trajectory)
            remoteDevice->wait(value);
    }

    if(runner.cycle != CannedCycle_NONE) //включен постоянный цикл
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
                case CannedCycle_NONE: //assume(0);
                case CannedCycle_RESET: //assume(0);
                case CannedCycle_SINGLE_DRILL:
                {
                    set_move_mode(MoveMode_LINEAR);
                    pos2.z = runner.cycleDeepLevel;
                    moveTo(pos2);
                    set_move_mode(MoveMode_FAST);
                    moveTo(pos);
                    break;
                }
                case CannedCycle_DRILL_AND_PAUSE:
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
                case CannedCycle_DEEP_DRILL:
                {
                    coord curZ = pos.z - runner.cycleStep;
                    for(; curZ > runner.cycleDeepLevel; curZ -= runner.cycleStep)
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
                    }
                    if(curZ != runner.cycleDeepLevel)
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
                    }

                    set_move_mode(MoveMode_FAST);
                    moveTo(pos);

                    break;
                }
            }

			to_global(pos);
            runner.position = pos;
        }
    }
    else if(runner.motionMode == MotionMode_FAST || runner.motionMode == MotionMode_LINEAR) //движение по прямой
    {
        Coords pos;
        if(readedFrame.absoluteSet)
        {
            pos = runner.position;
            get_readed_coord('X', pos.x);
            get_readed_coord('Y', pos.y);
            get_readed_coord('Z', pos.z);

            runner.position = pos;
            move_to(pos);
        }
        else if(get_new_position(pos))
        {
            runner.position = pos;
            move_to(pos);
        }
    }
    else if(runner.motionMode == MotionMode_CW_ARC || runner.motionMode == MotionMode_CCW_ARC)
    {
        if(readedFrame.absoluteSet)
            return InterError(InterError::WRONG_VALUE, "use absolute system G53 with arc");

        int ix, iy, iz;
        switch(runner.plane)
        {
            case Plane_NONE: //assume(0);
            case Plane_XY: ix = 0; iy = 1; iz = 2; break;
            case Plane_YZ: ix = 1; iy = 2; iz = 0; break;
            case Plane_ZX: ix = 2; iy = 0; iz = 1; break;
        }

        Coords centerPos;
        centerPos.x = centerPos.y = centerPos.z = 0;

        if(readedFrame.have_value('I') ||  //если задан центр круга
           readedFrame.have_value('J') ||
           readedFrame.have_value('K'))
        {
            if(readedFrame.have_value('R'))
                return InterError(InterError::WRONG_VALUE, "conflict parameter R with I,J,K offset");

            get_readed_coord('I', centerPos.x); //читаем центр круга
            get_readed_coord('J', centerPos.y);
            get_readed_coord('K', centerPos.z);

            centerPos.x += runner.position.x;
            centerPos.y += runner.position.y;
            centerPos.z += runner.position.z;

            Coords pos = runner.position;            //читаем, докуда двигаться
            get_new_position(pos);

            coord pitch = centerPos.r[iz] - runner.position.r[iz];//шаг винта
            centerPos.r[iz] = pos.r[iz];      //третий параметр означает нечто другое
            Coords planeCenter = centerPos;
            planeCenter.r[iz] = runner.position.r[iz];
            double radius = length(runner.position, planeCenter);

			auto len = length(pos, centerPos);
            if(fabs(radius - len) > remoteDevice->get_min_step()*2)//растяжение пока не поддерживается
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
                if(length(runner.position, pos) > remoteDevice->get_min_step()*2)
                    return InterError(InterError::WRONG_VALUE, "screw endpoint unreachable, start and end point have difference radius");

                runner.position = pos;
            }
            else
            {
                double angleMax = atan2(pos.r[iy] - planeCenter.r[iy], pos.r[ix] - planeCenter.r[ix]);
                angleMax -= angleStart;
                if(runner.motionMode == MotionMode_CCW_ARC)
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

                if(length(runner.position, pos) > remoteDevice->get_min_step()*2)
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
            if((radius < 0) == (runner.motionMode == MotionMode_CCW_ARC))
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
            if(runner.motionMode == MotionMode_CCW_ARC)
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
    if(center.x != runner.position.x && runner.plane == Plane_YZ)
        return true;
    if(center.y != runner.position.y && runner.plane == Plane_ZX)
        return true;
    if(center.z != runner.position.z && runner.plane == Plane_XY)
        return true;

    return false;
}

//====================================================================================================
//рисует винтовую линию или просто круг
void GCodeInterpreter::draw_screw(Coords center, double radius, double ellipseCoef,
                double angleStart, double angleMax, double angleToHeight,
                int ix, int iy, int iz)
{
    double accuracy = remoteDevice->get_min_step();
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
    if(runner.motionMode == MotionMode_CW_ARC)
        aScale = -1;

    Coords curPos;
    double angle = 0;
    for(; angle < angleMax; angle += step)
    {
        curPos.r[ix] = center.r[ix] + radius * cos(angleStart + angle * aScale);
        curPos.r[iy] = center.r[iy] + radius * sin(angleStart + angle * aScale) * ellipseCoef;
        curPos.r[iz] = center.r[iz] + angle * angleToHeight;
        move_to(curPos);
    }
    angle = angleMax;
    curPos.r[ix] = center.r[ix] + radius * cos(angleStart + angle * aScale);
    curPos.r[iy] = center.r[iy] + radius * sin(angleStart + angle * aScale);
    curPos.r[iz] = center.r[iz] + angle * angleToHeight;
    move_to(curPos);
    runner.position = curPos;
}

//====================================================================================================
//чтение новых координат с учётом модальных кодов
bool GCodeInterpreter::get_new_position(Coords &pos)
{
    if(readedFrame.have_value('X') ||
       readedFrame.have_value('Y') ||
       readedFrame.have_value('Z'))
    {
        if(runner.incremental)
        {
            pos.x = pos.y = pos.z = 0;
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

        if(runner.incremental)
        {
            for(int i = 0; i < NUM_COORDS; ++i)
                pos.r[i] += runner.position.r[i];
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

    auto &cs = runner.csd[runner.coordSystemNumber];
    for(int i = 0; i < NUM_COORDS; ++i)
        coords.r[i] += cs.pos0.r[i];
}

//====================================================================================================
//получение локальных координат из глобальных
void GCodeInterpreter::to_local(Coords &coords)
{
    if(runner.coordSystemNumber == -1)
        return;

    auto &cs = runner.csd[runner.coordSystemNumber];
    for(int i = 0; i < NUM_COORDS; ++i)
        coords.r[i] -= cs.pos0.r[i];
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
    if(runner.units == UnitSystem_INCHES)
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

    runner.motionMode = MotionMode_FAST;
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
      const double LATENCY = 0.2; //0.2 секунд на остановку после отпускания кнопки
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
    for(auto iter = inputFile.begin(); iter != inputFile.end(); ++iter)
    {
        if (!trajectory)
            remoteDevice->set_current_line(lineNumber);
        auto result = execute_frame(iter->c_str());
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
    runner.feed = 100; //отфига
    runner.incremental = false;
    runner.motionMode = MotionMode_FAST;
    runner.deviceMoveMode = MoveMode_FAST;
    runner.cycle = CannedCycle_NONE;
    runner.plane = Plane_XY;
    runner.offset.x = 0;
    runner.offset.y = 0;
    runner.offset.z = 0;
    runner.position.x = 0;
    runner.position.y = 0;
    runner.position.z = 0;
    runner.units = UnitSystem_METRIC;
    memset(&runner.csd, 0, sizeof(runner.csd));
    coordsInited = false;
}
