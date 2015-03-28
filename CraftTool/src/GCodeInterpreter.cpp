#include "GCodeInterpreter.h"

using namespace Interpreter;

#define MM_PER_INCHES 2.54

coord length(Coords from, Coords to)
{
    Coords delta;
    delta.x = from.x - to.x;
    delta.y = from.y - to.y;
    delta.z = from.z - to.z;
    return sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
}

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
        case 'S': return BitPos_S;
        case 'R': return BitPos_R;
        case 'D': return BitPos_D;
        case 'L': return BitPos_L;

        default: return BitPos_ERR;
    }
}

InterError FrameParams::set_value(char letter, double value)
{
    BitPos index = get_bit_pos(letter);
    if (index == BitPos_ERR)
        return InterError_INVALID_STATEMENT;

    if(flagValue.get(index))
        return InterError_DOUBLE_DEFINITION;

    flagValue.set(index, true);
    this->value[int(index)] = value;

    return InterError_ALL_OK;
}


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


bool FrameParams::have_value(char letter)
{
    BitPos index = get_bit_pos(letter);
    if (index == BitPos_ERR)
        return false;

    return flagValue.get(index);
}


GCodeInterpreter::GCodeInterpreter(void)
{
}


GCodeInterpreter::~GCodeInterpreter(void)
{
}

InterError GCodeInterpreter::execute_frame(const char *frame)
{
    //char letter;
    //double value;

    InterError state = InterError_ALL_OK;

    state = reader.parse_codes(frame); //проверяем строку на валидность, читаем значения в массив
    if(state != InterError_ALL_OK) return state;

    state = make_new_state(); //читаем все коды и по ним создаём команды изменения состояния
    if(state != InterError_ALL_OK) return state;

    state = run_modal_groups(); //исполняем коды или пересылаем их устройству
    if(state != InterError_ALL_OK) return state;

    //state = move_tool(); //если заданы координаты, шлём их устройству
    //if(state != ALL_OK) return state;

    return InterError_ALL_OK;
}

//читает данные из строки в массив
InterError Reader::parse_codes(const char *frame)
{
    codes.clear();
    position = 0;
    string = frame;
    state = InterError_ALL_OK;

    GKey current;
    current.position = position;
    while(parse_code(current.letter, current.value))
    {
        codes.push_back(current);
        current.position = position;
    }

    return state;
}

//очищает прочитанные данные фрейма
void FrameParams::reset()
{
    flagValue.reset();
    flagModal.reset();
    sendWait = false; //G4 readed
    plane = Plane_NONE;    //G17
    motionMode = MotionMode_NONE;
}

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
                return InterError_DOUBLE_DEFINITION;
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

                    case 53: runner.coordSystemNumber = -1; break;

                    case 54: case 55: case 56: case 57: case 58:
                        runner.coordSystemNumber = intValue - 54; break;

                    //case 80: runner.motionMode = MotionMode_GO_BACK; break;

                    case 90: runner.incremental = false; break;
                    case 91: runner.incremental = true; break;

                    default: return InterError_INVALID_STATEMENT;
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
            case 'F':
            case 'S':
            case 'R':
            case 'D':
            case 'L':
                readedFrame.set_value(iter->letter, iter->value);
                break;

            default: return InterError_INVALID_STATEMENT;
        }
    }

    return InterError_ALL_OK;
}

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

//исполняет прочитанный фрейм в нужном порядке
InterError GCodeInterpreter::run_modal_groups()
{
    double value;

    switch (readedFrame.plane) //задаём плоскость обработки
    {
        case Plane_NONE: break;

        case Plane_XY:
            remoteDevice->set_plane(MovePlane_XY);
            runner.plane = MovePlane_XY;
            break;
        case Plane_ZX:
            remoteDevice->set_plane(MovePlane_ZX);
            runner.plane = MovePlane_ZX;
            break;
        case Plane_YZ:
            remoteDevice->set_plane(MovePlane_YZ);
            runner.plane = MovePlane_YZ;
            break;

        default: return InterError_WRONG_PLANE;
    }

    //if(readedFrame.get_value('S', value)) //скорость вращения шпинделя
    //    ;

    if(readedFrame.get_value('F', value)) //скорость подачи
        remoteDevice->set_feed(value);

    switch (readedFrame.motionMode)
    {
        case MotionMode_NONE: break;

        case MotionMode_CCW_ARC:
        case MotionMode_CW_ARC:
        case MotionMode_FAST:
        case MotionMode_LINEAR:
            runner.motionMode = readedFrame.motionMode;
            break;

        //case MotionMode_GO_BACK:
        //    break;
    }

    switch (readedFrame.motionMode)
    {
        case MotionMode_CCW_ARC: remoteDevice->set_move_mode(MoveMode_CCW_ARC); break;
        case MotionMode_CW_ARC:  remoteDevice->set_move_mode(MoveMode_CW_ARC); break;
        case MotionMode_FAST:    remoteDevice->set_move_mode(MoveMode_FAST); break;
        case MotionMode_LINEAR:  remoteDevice->set_move_mode(MoveMode_LINEAR); break;
        case MotionMode_NONE: ;
    }

    if(readedFrame.sendWait)
    {
        if(!readedFrame.get_value('P', value))
            return InterError_NO_VALUE;

        if(value < 0)
            return InterError_WRONG_VALUE;

        remoteDevice->wait(value);
    }

    if(runner.motionMode == MotionMode_FAST || runner.motionMode == MotionMode_LINEAR) //движение по прямой
    {
        Coords pos;
        if(get_new_position(pos))
        {
            runner.position = pos;
            remoteDevice->set_position(pos.x, pos.y, pos.z);
        }
    }
    else if(runner.motionMode == MotionMode_CW_ARC || runner.motionMode == MotionMode_CCW_ARC)
    {
        Coords centerPos;
        centerPos.x = centerPos.y = centerPos.z = 0;

        if(readedFrame.have_value('I') ||  //если задан центр круга
           readedFrame.have_value('J') ||
           readedFrame.have_value('K'))
        {
            readedFrame.get_value('I', centerPos.x); //читаем центр круга
            readedFrame.get_value('J', centerPos.y);
            readedFrame.get_value('K', centerPos.z);

            local_deform(centerPos);
            centerPos.x += runner.position.x;
            centerPos.y += runner.position.y;
            centerPos.z += runner.position.z;

            Coords pos = runner.position;            //читаем, докуда двигаться
            get_new_position(pos);

            if(fabs(length(runner.position, centerPos) - length(pos, centerPos)) > 0.001)//растяжение пока не поддерживается
                return InterError_WRONG_VALUE;

            bool isScrew = false;
            if(centerPos.x != runner.position.x && runner.plane == MovePlane_YZ)
                isScrew = true;
            if(centerPos.y != runner.position.y && runner.plane == MovePlane_ZX)
                isScrew = true;
            if(centerPos.z != runner.position.z && runner.plane == MovePlane_XY)
                isScrew = true;

            if(isScrew) //винтовая интерполяция пока не поддерживается
                return InterError_WRONG_VALUE;

            /*double rads[3]; //радиусы окружности

            switch(runner.plane)
            {
                case MovePlane_XY:
                    rads[0] =
                break;
                case MovePlane_XY:
                break;
                case MovePlane_XY:
                break;
            }*/

            runner.position = pos;
            remoteDevice->set_circle_position(pos.x, pos.y, pos.z, centerPos.x, centerPos.y, centerPos.z);
        }
    }

    return InterError_ALL_OK;
}

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
        if(readedFrame.get_value('X', pos.x))
            pos.x = to_mm(pos.x);
        if(readedFrame.get_value('Y', pos.y))
            pos.y = to_mm(pos.y);
        if(readedFrame.get_value('Z', pos.z))
            pos.z = to_mm(pos.z);

        //local_deform(pos);
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

//сдвиг в глобальные координаты
void GCodeInterpreter::to_global(Coords &coords)
{
    if(runner.coordSystemNumber == -1)
        return;

    auto &cs = runner.csd[runner.coordSystemNumber];
    for(int i = 0; i < NUM_COORDS; ++i)
        coords.r[i] += cs.pos0.r[i];
}

//получение локальных координат из глобальных
void GCodeInterpreter::to_local(Coords &coords)
{
    if(runner.coordSystemNumber == -1)
        return;

    auto &cs = runner.csd[runner.coordSystemNumber];
    for(int i = 0; i < NUM_COORDS; ++i)
        coords.r[i] -= cs.pos0.r[i];
}

//преобразования локальных координат
void GCodeInterpreter::local_deform(Coords &coords)
{
    coords = to_mm(coords);

    if(runner.coordSystemNumber == -1)
        return;
}

//перевод в мм
coord GCodeInterpreter::to_mm(coord value)
{
    if(runner.units == UnitSystem_INCHES)
        value *= MM_PER_INCHES;
    return value;
}

Coords GCodeInterpreter::to_mm(Coords value)
{
    for(int i = 0; i < NUM_COORDS; ++i)
        value.r[i] = to_mm(value.r[i]);
    return value;
}

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
        state = InterError_WRONG_LETTER;
        return false;
    }

    position++;

    find_significal_symbol();
    if (!parse_value(value))
    {
        state = InterError_WRONG_VALUE;
        return false;
    };

    return true;
}

//пропускает пробелы
void Reader::accept_whitespace()
{
    while (string[position] == ' ' || string[position] == '\t') position++;
}

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

//читает число
bool Reader::parse_value(double &dst)
{
    const char *cursor = string + position;

    double value = 0;
    int sign = 1;       // +-
    int numDigits = 0;  //сколько цифр прочитано
    int maxDigits = 10; //сколько всего можно
    int denominator = 1;//на сколько поделить прочитанное

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

//читает строки в список
void GCodeInterpreter::execute_file()
{
    int lineNumber = 0;
    for(auto iter = inputFile.begin(); iter != inputFile.end(); ++iter)
    {
        remoteDevice->set_current_line(lineNumber);
        auto result = execute_frame(iter->c_str());
        if(result != InterError_ALL_OK)
            std::cout<<"execute_frame error "<<result<<", line" << lineNumber << "\n";
        ++lineNumber;
    };
}

void GCodeInterpreter::init()
{
    //reader.position = {0.0f, 0.0f, 0.0f};
    reader.state = InterError_ALL_OK;

    runner.coordSystemNumber = -1;
    runner.cutterLength = 0;
    runner.cutterRadius = 0;
    runner.feed = 100; //отфига
    runner.incremental = false;
    runner.motionMode = MotionMode_FAST;
    runner.plane = MovePlane_XY;
    runner.offset.x = 0;
    runner.offset.y = 0;
    runner.offset.z = 0;
    runner.position.x = 0;
    runner.position.y = 0;
    runner.position.z = 0;
    runner.units = UnitSystem_METRIC;
    memset(&runner.csd, 0, sizeof(runner.csd));

    runner.csd[0].pos0.x = 100; //FIXME  for test
    runner.csd[0].pos0.y = 50;
}
