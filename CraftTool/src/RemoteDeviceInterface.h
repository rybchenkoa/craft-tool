#pragma once

#include "Coords.h"

#define NUM_COORDS 4 //сколько координат задаем в G-коде
#define MAX_IN_PINS 11 //количество входных пинов

enum MoveMode :char //режим движения/интерполяции
{
	MoveMode_LINEAR = 0,
	MoveMode_HOME,
	MoveMode_FAST,
};

// устройство, выполняющее команды интерпретатора
class RemoteDeviceInterface
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
