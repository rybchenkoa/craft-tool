#pragma once

#include <QObject>
#include "UniversalConnection.h"
#include "Coords.h"

#define NUM_COORDS 4 //сколько координат задаем в G-коде
#define MAX_IN_PINS 11 //количество входных пинов

enum MoveMode :char //режим движения/интерполяции
{
	MoveMode_LINEAR = 0,
	MoveMode_HOME,
	MoveMode_FAST,
};

enum SwitchGroup :char
{
	SwitchGroup_MIN = 0,
	SwitchGroup_MAX,
	SwitchGroup_HOME,
};

//для теста надо будет сделать отдельную реализацию класса
class IRemoteDevice
{
public:
    virtual void init()=0; //сбрасываем очередь команд, ищем начало координат и т.п.
    virtual void reset_packet_queue()=0;
    virtual void set_move_mode(MoveMode mode)=0; //задаём режим интерполяции
    virtual void set_position(Coords position)=0; //перемещаем фрезу
    virtual void wait(double time)=0; //задержка
    virtual void set_velocity_and_acceleration(double velocity[MAX_AXES], double acceleration[MAX_AXES])=0; //задать скорость и ускорение
    virtual double get_max_velocity(int coord)=0;
    virtual double get_max_acceleration(int coord)=0;
    virtual void set_feed(double feed)=0; //скорость подачи (скорость движения при резке)
    virtual void set_feed_multiplier(double multiplier)=0; //множитель скорости подачи
    virtual void set_feed_normal()=0; //подача в мм/сек
    virtual void set_feed_per_rev(double feed)=0; //подача в мм/оборот
    virtual void set_feed_stable(double frequency)=0; //стабилизация оборотов шпинделя
    virtual void set_feed_sync(double step, double pos, int axe)=0; //синхронизация оси со шпинделем
    virtual void set_feed_throttling(bool enable, int period, int size)=0; //движение рывками
    virtual void set_feed_adc(bool enable)=0; //управление подачей через напряжение
	
	virtual void set_spindle_vel(double feed)=0; //скорость подачи (скорость движения при резке)
    virtual void set_step_size(double stepSize[MAX_AXES])=0; //длина одного шага
    virtual void pause_moving(bool needStop)=0; //временная остановка движения
    virtual void break_queue()=0; //полная остановка с прерыванием программы
	virtual void homing()=0; //отъехать к концевикам и задать машинные координаты

    virtual int  queue_size() = 0; //длина очереди команд

    virtual void set_current_line(int line)=0; //задаёт номер строки, для которой сейчас будут вызываться команды
    virtual int  get_current_line()=0; //возвращает номер строки, для которой сейчас исполняются команды
    virtual const Coords* get_current_coords()=0; //последние принятые координаты
    virtual double get_min_step(int axis1, int axis2)=0; //точность устройства
};

//класс передаёт команды по com-порту на микроконтроллер, а он уже дальше интерполирует
class CRemoteDevice : public QObject, public IRemoteDevice
{
    Q_OBJECT
public:
    CRemoteDevice();
    ~CRemoteDevice();

    void init() override;
    void reset_packet_queue() override;
    void set_move_mode(MoveMode mode) override;
    void set_position(Coords position) override;
    void wait(double time) override;
    void set_velocity_and_acceleration(double velocity[MAX_AXES], double acceleration[MAX_AXES]) override;
    double get_max_velocity(int coord) override;
    double get_max_acceleration(int coord) override;
    void set_feed(double feed) override;
    void set_feed_multiplier(double multiplier) override;
    void set_feed_normal() override;
    void set_feed_per_rev(double feed) override;
    void set_feed_stable(double frequency) override;
    void set_feed_sync(double step, double pos, int axe) override;
    void set_feed_throttling(bool enable, int period, int size) override;
    void set_feed_adc(bool enable) override;
    void set_step_size(double stepSize[MAX_AXES]) override;
	void set_spindle_vel(double feed) override;
    void pause_moving(bool needStop) override;
    void break_queue() override;
	void homing() override;

    void set_fract();
	void try_set_fract(const Coords& delta);
	int calculate_new_delta(Coords& delta, const Coords& pos);
	void calculate_moving_params(const Coords& delta, double& length, double& velocity, double& acceleration) const;
	Coords to_hard(const Coords& pos) const;

    int  queue_size() override;

    void set_current_line(int line) override;
    int  get_current_line() override;
    const Coords* get_current_coords() override;
    double get_min_step(int axis1, int axis2) override;
    int send_lag_ms();

    bool on_packet_received(char *data, int size);
    bool process_packet(char *data, int size);

	bool inited;                       //порт нужен для инициализации, но до инициализации нельзя принимать с него лишних данных
	UniversalConnection *connection;   //подключение к удалённому устройству
    int missedSends;                   //пакет послан, ответ не получен
    int missedReceives;                //принят битый пакет
    int missedHalfSend;                //принято сообщение о битом пакете
    int packSends;                     //послано пакетов

    //текущее состояние
    double scale[MAX_AXES];            //шагов на миллиметр
	double stepSize[MAX_AXES];         //длина одного шага
    double minStep;                    //макс. точность устройства
    double secToTick;                  //тиков таймера в одной секунде
    Coords lastPosition;               //последняя переданная позиция
    Coords lastDelta;                  //последний вектор сдвига
    double feed;                       //подача
	double spindleSpeed;               //скорость шпинделя
    MoveMode moveMode;                 //режим перемещения
    double velocity[MAX_AXES];         //максимальная скорость по каждой оси
    double acceleration[MAX_AXES];     //максимальное ускорение по каждой оси
    double fractValue;                 //насколько должна измениться траектория, чтобы считать её новой линией
    bool fractSended;                  //послан ли уже излом траектории
	bool usedCoords[MAX_AXES];         //используемые интерпретатором координаты
	bool usedAxes[MAX_AXES];           //используемые станком оси
	int  slaveAxes[MAX_AXES];          //связанная ось (к одной оси можно привязать еще только одну)
	std::vector<int> toDeviceIndex;    //перевод номеров координат в номера генераторов сигнала на устройстве
	std::vector<int> fromDeviceIndex;  //обратное преобразование при получении координат с устройства
	bool invertAxe[MAX_AXES];          //инвертировать ли ось
	int switchPolarity;                //биты - значения активного уровня сигнала
	int  switchMin[MAX_AXES];          //концевик на минимум
	int  switchMax[MAX_AXES];          //концевик на максимум
	int  switchHome[MAX_AXES];         //концевик для дома
	double backHome[MAX_AXES];         //на сколько отъехать от дома
	Coords coordHome;                  //какие задать координаты дому
	std::string homingScript;          //как ехать к дому

    //состояние удалённого устройства
    Coords currentCoords;              //текущие координаты

protected:
	void set_switches(SwitchGroup group, int pins[MAX_AXES]);
	void set_spindle_params(int marksCount, int marksPin, int marksFrequency, int filterSize);
	void set_coord(Coords pos, bool used[MAX_AXES]);
	void set_pwm_freq(double fast, double slow);

    template<typename T>
    void push_packet_common(T *packet);

    template<typename T>
    void push_packet_modal(T *packet);

    void send_thread();
    std::thread sendThread;
    bool stop_token = false;

    struct ConnectData;
    std::unique_ptr<ConnectData> commands[2]; //очередь для g-кода и для сервисных команд
    std::mutex queueMutex;             //защита очереди от порчи
    std::condition_variable eventQueueAdd;   //в очередь добавлен пакет
    std::condition_variable eventPacketReceived;   //сообщение о принятии пакета

    int pushLine;                 //строка, из которой читаются команды
    int workLine;                 //строка, команда которой сейчас выполняется

signals:
    void coords_changed(float x, float y, float z);

};
