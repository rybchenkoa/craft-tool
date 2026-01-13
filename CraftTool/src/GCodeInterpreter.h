#pragma once

#include "IRemoteDevice.h"
#include "GCodeLexer.h"
#include "GCodeFrameParser.h"

namespace Interpreter
{
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
	bool feedThrottling;     //включена ли прерывистая подача
	bool feedAdc;            //включено ли управление подачей напряжением
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

// интерпретатор G-кода
class GCodeInterpreter
{
public:

    std::list<std::string> inputFile;     //строки входного файла

    Runner runner;            //исполнитель команд
	GCodeLexer lexer;         //парсер команд
	GCodeFrameParser readedFrame; //прочитанные команды одной строки
    IRemoteDevice *remoteDevice; //устройство, которое исполняет команды
    Trajectory *trajectory;      //или массив, в который заносятся точки пути
    bool coordsInited;        //при начальной инициализации выставляем интерпретатору координаты устройства

    void init();                            //инициализация для нового файла
    bool read_file(const char *name);             //запоминает строки текстового файла
    void execute_file(Trajectory *trajectory);                    //исполняет файл
	void execute_line(std::string line);    //исполняет одну строку
    InterError execute_frame(const char *frame);    //выполнение строки
    InterError run_modal_groups();          //запуск команд
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
