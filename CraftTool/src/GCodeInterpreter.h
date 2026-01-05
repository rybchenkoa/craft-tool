#pragma once

#include "IRemoteDevice.h"

namespace Interpreter
{

enum class UnitSystem //система единиц
{
	METRIC, //метрическая
	INCHES, //дюймовая
};

enum class FeedMode // режим контроля подачи
{
	NONE = 0,
	PER_MIN,    // подача в минуту, G94
	PER_REV,    // подача на оборот, G95
	STABLE_REV, // стабилизация оборотов, G95.1
	THROTTLING, // прерывистая подача, G94.1
};

enum class MotionMode //режимы перемещения
{
	NONE = 0,
	FAST,      //быстрое позиционирование
	LINEAR,    //линейная интерполяция
	CW_ARC,    //круговая интерполяция
	CCW_ARC,
	LINEAR_SYNC, //нарезание резьбы
};

enum class CannedCycle
{
	NONE = 0,
	RESET,           //отмена цикла, G80
	SINGLE_DRILL,    //простое сверление, G81
	DRILL_AND_PAUSE, //сверление с задержкой на дне, G82
	DEEP_DRILL,      //сверление итерациями, G83
};

enum class CannedLevel
{
	NONE = 0,
	HIGH,   //отвод к исходной плоскости, G98
	LOW,    //отвод к плоскости обработки, G99
};

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

enum class Plane
{
	NONE = 0,
	XY,
	ZX,
	YZ,
};

struct InterError
{
	enum Code
	{
		ALL_OK = 0,
		INVALID_STATEMENT, //неизвестная буква
		DOUBLE_DEFINITION, //буква повторилась
		WRONG_LETTER,
		WRONG_VALUE,
		NO_VALUE,
	};

	Code code;
	std::string description;
	InterError() :code(ALL_OK){};
	InterError(Code code, std::string description) : code(code), description(description) {};
};

//параметры координатной системы
struct CoordSystemData
{
    Coords pos0;               //начало отсчёта в глобальной системе координат
};

//отрисовываемые отрезки
struct TrajectoryPoint
{
    bool isFast;
    Coords position;
};

using Trajectory = std::vector<TrajectoryPoint>;
//=================================================================================================
//интерпретатор работает следующим образом
//читается вся строка, выбираются команды и для них ищутся параметры

//здесь переменные для выполнения команд
struct Runner
{
    //параметры, нужные для кодов, которые действуют на несколько строк
    Coords position;         //"текущая" позиция устройства в миллиметрах
    UnitSystem units;        //текущая система единиц измерения
    bool incremental;        //абсолютная система координат?
    MotionMode motionMode;   //режим перемещения (линейная интерполяция и т.п.)
    MoveMode deviceMoveMode; //режим перемещения устройства
    Plane plane;             //текущая плоскость интерполяции
	FeedMode feedMode;       //режим управления подачей (в минуту, на оборот)
	FeedMode feedModeRollback; //режим подачи, к которому откатывать после G32
	double feed;             //подача в мм/мин
	double feedPerRev;       //подача в мм/оборот
	double feedStableFreq;   //подача со стабилизацией оборотов
	double spindleAngle;     //угол поворота шпинделя (в оборотах)
	double threadPitch;      //шаг витка (мм/оборот)
	int threadIndex;         //координата, с которой синхронизирован шпиндель
    double cutterRadius;     //радиус фрезы
    double cutterLength;     //длина фрезы
    int coordSystemNumber;   //номер выбранной координатной системы
    CoordSystemData csd[5];  //параметры команд G54..G58

    CannedCycle cycle;       //текущий цикл
    bool   cycleUseLowLevel; //использовать R вместо стартовой точки
    bool   cycle81Incremental; //в g81 Z вместо конечной координаты задаёт смещение от R
    double cycleLowLevel;    //плоскость отвода (задаётся в R)
    double cycleHiLevel;     //исходная плоскость задаётся в стартовом Z
    double cycleDeepLevel;   //глубина сверления задаётся в Z
    double cycleStep;        //глубина одного шага Q
    int    cycleWait;        //задержка в цикле P
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
    CannedCycle cycle;//G80-G83
    CannedLevel cycleLevel; //G98, G99

    MotionMode motionMode;
	FeedMode feedMode;

    FrameParams()
    {
        reset();
    }

    void reset();

    BitPos get_bit_pos(char letter);

    InterError set_value(char letter, double value);   //для кодов, имеющих значение, сохраняем это значение
    bool get_value(char letter, double &value);        //забираем считанное значение
    bool have_value(char letter);                      //есть ли значение

};

// интерпретатор G-кода
class GCodeInterpreter
{
public:

    std::list<std::string> inputFile;     //строки входного файла

    Runner runner;            //исполнитель команд
    Reader reader;            //парсер команд
    FrameParams readedFrame;  //прочитанные команды одной строки
    IRemoteDevice *remoteDevice; //устройство, которое исполняет команды
    Trajectory *trajectory;      //или массив, в который заносятся точки пути
    bool coordsInited;        //при начальной инициализации выставляем интерпретатору координаты устройства

    void init();                            //инициализация для нового файла
    bool read_file(const char *name);             //запоминает строки текстового файла
    void execute_file(Trajectory *trajectory);                    //исполняет файл
	void execute_line(std::string line);    //исполняет одну строку
    InterError execute_frame(const char *frame);    //выполнение строки
    InterError make_new_state();            //чтение команд из строки
    InterError run_modal_groups();          //запуск команд
    ModalGroup get_modal_group(char letter, double value); //возвращает модальную группу команды
	InterError run_feed_mode();             //обработка режима подачи
	InterError run_feed_rate();             //обработка значения подачи
	void update_motion_mode();              //читает режим перемещения
	InterError update_cycle_params();       //читает параметры циклов
	InterError run_dwell();                 //обработка паузы
    InterError run_modal_group_linear();    //обработка линейного движения
    InterError run_modal_group_arc();       //обработка движения по дуге
    InterError run_modal_group_cycles();    //обработка цикла
	InterError run_linear_sync(Coords& pos); //задаёт параметры линейной синхронизации
    void set_move_mode(MoveMode mode);      //изменение режима перемещения
    void local_deform(Coords &coords);      //преобразование масштаба, поворот в локальной системе координат
    void to_global(Coords &coords);         //сдвиг в глобальные координаты
    void to_local(Coords &coords);          //сдвиг в локальные координаты
    coord to_mm(coord value);               //переводит из текущих единиц в мм
    Coords to_mm(Coords value);
    void move_to(Coords position);          //линейное перемещение
    bool is_screw(Coords center);           //проверяет, что траектория будет винтовой
    void draw_screw(Coords center, double radius, double ellipseCoef,
                    double angleStart, double angleMax, double angleToHeight,
                    int ix, int iy, int iz);//рисует винтовую линию

    bool get_readed_coord(char letter, coord &value); //сразу переводит единицы измерения
    bool get_new_position(Coords &pos);  //чтение новых координат с учётом модальных кодов

    void move(int coordNumber, coord add, bool fast); //ручное перемещение
	std::vector<std::string> get_active_codes();

    GCodeInterpreter(void);
    ~GCodeInterpreter(void);
};

}
