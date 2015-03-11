#pragma once

#include <vector>
#include <list>
#include <string>
#include <iostream>
#include <fstream>
#include "IRemoteDevice.h"

namespace Interpreter
{

typedef double coord;//чтобы не путаться, координатный тип введём отдельно

struct Coords   //все координаты устройства
{
    union
    {
        struct
        {
            coord x, y, z;
        };
        struct
        {
            coord r[NUM_COORDS];
        };
    };
};

enum UnitSystem //система единиц
{
    UnitSystem_METRIC, //метричекая
    UnitSystem_INCHES, //дюймовая
};

enum CoordIndex //номера координат в массиве
{
    X_AXIS = 0,
    Y_AXIS = 1,
    Z_AXIS = 2,
};
/*
enum BitPos //позиции битов во флаговых переменных
{
    X = 0,
    Y = 1,
    Z = 2,

    I = 0,
    J = 1,
    K = 2,
};
*/
enum MotionMode //режимы перемещения
{
    MotionMode_NONE = 0,
    MotionMode_FAST,      //быстрое позиционирование
    MotionMode_LINEAR,    //линейная интерполяция
    MotionMode_CW_ARC,    //круговая интерполяция
    MotionMode_CCW_ARC,
    MotionMode_GO_BACK,   //возврат
};

enum OpCode  //распознаваемые коды операций
{
    OpCode_NONE = 0,
    G0_FAST_MOVE,
    G1_LINEAR,
    G2_CW_ARC,
    G3_CCW_ARC,
    G4_PAUSE,
    G6_PARABOLIC,
    G8_ACCELERATE,
    G9_DECELERATION,
    G17_XY,
    G18_ZX,
    G19_YZ,
    G20_INCH,
    G21_METRIC,
    G27_REFERENCE_POINT,
    G40_RESET_CORRECTION_RADIUS,
    G41_CORRECTION_LEFT,  //по часовой
    G42_CORRECTION_RIGHT,
    G43_CORRECTION_POSITIVE, //прибавить длину инструмента
    G44_CORRECTION_NEGATIVE,
    G49_RESET_CORRECTION_LENGTH,
    //G50_MIRROR,
    //G501_MIRROR_RESET,
    G51_SCALE,
    G511_SCALE_RESET,
    G52_LOCAL_SYSTEM,
    G53_RESET_LOCAL_SYSTEM,
    G90_ABSOLUTE_MOVES,
    G91_INCREMENTAL_MOVES,
};

enum ModalGroup //некоторые операторы не могут одновременно содержаться в одном фрейме
{
    ModalGroup_NONE = 0, //g4,g10,g28,g30,g53,g92.[0-3]
    ModalGroup_MOVE,                 //g0..g3 //G38.2, G80..G89
    ModalGroup_INCREMENTAL, //g90..g91
    ModalGroup_UNITS, //g20..g21
    //ModalGroup_CYCLE, //g80..g85
    ModalGroup_COORD_SYSTEM, //g54..g58
    ModalGroup_TOOL_LENGTH_CORRECTION, //g43,g44,g49
    ModalGroup_TOOL_RADIUS_CORRECTION, //g40..g42
    ModalGroup_CYCLE_RETURN, //g98, g99
    ModalGroup_ACTIVE_PLANE, //g17..g19
    ModalGroup_STOP, //M0, M1, M2, M30, M60
    ModalGroup_TOOL_CHANGE, //M6
    ModalGroup_TURN_TOOL, //M3, M4, M5
    ModalGroup_GREASER, //M7, M8, M9
};

enum Plane
{
    Plane_NONE = 0,
    Plane_XY,
    Plane_ZX,
    Plane_YZ,
};

enum InterError
{
    InterError_ALL_OK = 0,
    InterError_INVALID_STATEMENT, //неизвестная буква
    InterError_DOUBLE_DEFINITION, //буква повторилась
    InterError_WRONG_PLANE,       //задана неправильная плоскость
    InterError_WRONG_LETTER,
    InterError_WRONG_VALUE,
    InterError_NO_VALUE,
};

//=================================================================================================
//интерпретатор работает следующим образом
//берётся команда, читаются её параметры
//команда посылается на выполнение
//это значит что X10 G0 писать нельзя (и даже если где-то можно, то это выглядит как г-окод и нефиг такое поддерживать)

//здесь переменные для выполнения команд
struct Runner
{
    //параметры, нужные для кодов, которые действуют на несколько строк
    Coords position;         //"текущая" позиция устройства в миллиметрах
    UnitSystem units;        //текущая система единиц измерения
    bool incremental;     //абсолютная система координат?
    MotionMode motionMode;   //режим перемещения (линейная интерполяция и т.п.)
    MovePlane plane;         //текущая плоскость интерполяции
    double feed;             //подача в мм/мин
    double cutterRadius;     //радиус фрезы
    double cutterLength;     //длина фрезы
    int coordSystemNumber;   //номер выбранной координатной системы

    Coords offset;           //отступ для ручного задания положения
};

struct GKey
{
    char letter;
    double value;
    int position;
};

//здесь переменные для чтения команд
struct Reader 
{
    const char *string;
    int  position;
    InterError  state;
    std::vector<GKey> codes;

    InterError parse_codes(const char *frame); //читает коды и значения параметров

    bool parse_code(char &letter, double &value); //следующий код
    void accept_whitespace(); //пропускает пробелы
    void find_significal_symbol(); //пропускает комментарии, пробелы
    bool parse_value(double &value); //считывает число
};

enum BitPos
{
    BitPos_ERR = -1,
    BitPos_X=0,BitPos_Y,BitPos_Z,
    BitPos_A,BitPos_B,BitPos_C,
    BitPos_I,BitPos_J,BitPos_K,
    BitPos_F,BitPos_P,BitPos_S,BitPos_R,BitPos_D,BitPos_L,
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
struct FrameParams
{
    static const int flags = 256/32;

    double value[32];  //значения для кодов, имеющих значения
    Flags flagValue;          //битовый массив

    //int modalCodes[12]; //по числу модальных команд
    Flags flagModal;

    bool sendWait;    //G4 readed
    Plane plane;      //G17
    bool absoluteSet; //G53

    MotionMode motionMode;

    FrameParams()
    {
        reset();
    }

    void reset();

    BitPos get_bit_pos(char letter);

    InterError set_value(char letter, double value);   //для кодов, имеющих значение, сохраняем это значение
    bool get_value(char letter, double &value);        //забираем считанное значение

};

class GCodeInterpreter  //несомненно, это интерпретатор г-кода )
{
public:

    std::list<std::string> inputFile;     //строки входного файла

    Runner runner;            //исполнитель команд
    Reader reader;            //парсер команд
    FrameParams readedFrame;  //прочитанные команды одной строки
    IRemoteDevice *remoteDevice; //устройство, которое исполняет команды

    void init();                            //инициализация для нового файла
    bool read_file(const char *name);             //запоминает строки текстового файла
    void execute_file();                    //исполняет файл
    InterError execute_frame(const char *frame);    //выполнение строки
    InterError make_new_state();            //чтение команд из строки
    InterError run_modal_groups();          //запуск команд
    ModalGroup get_modal_group(char letter, double value); //возвращает модальную группу команды


    //функции чтения и исполнения команд с параметрами
    void run_G0(); //XYZ
    void run_G1(); //XYZ F
    void run_G2(); //XYZ IJK
    void run_G3(); //XYZ IJK
    void run_G4(); //X
    void run_G5D1();
    void run_G6();
    void run_G41();
    void run_G42();
    void run_G43();
    void run_G44();
    GCodeInterpreter(void);
    ~GCodeInterpreter(void);
};

};
