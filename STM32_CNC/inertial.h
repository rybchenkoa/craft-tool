#pragma once
// обрабатывает изменение скорости с учётом максимального ускорения

#include "sys_timer.h"


struct Inertial
{
	float maxFeedVelocity;	//скорость подачи, мм/такт
	float acceleration;		//ускорение, мм/такт^2
	float velocity;			//скорость на прошлом шаге, мм/такт
	int state;			//ускорение/замедление/стабильное движение
	int lastTime;		//предыдущее время
	float lastVelocity; //предыдущая скорость

	//=====================================================================================================
	void stop()
	{
		velocity = 0;
		state = 0;
	}

	//=====================================================================================================
	void set_max_params(float maxVelocity, float maxAcceleration)
	{
		maxFeedVelocity = maxVelocity;
		acceleration = maxAcceleration;
	}

	//=====================================================================================================
	void start_acceleration()
	{
		lastVelocity = velocity;
		lastTime = timer.get_ticks();
	}

	//=====================================================================================================
	void accelerate(int multiplier)
	{
		int currentTime = timer.get_ticks();
		int delta = currentTime - lastTime;
		velocity = lastVelocity + delta * multiplier * acceleration;
		if (delta > 100000)
		{
			lastVelocity = velocity;
			lastTime = currentTime;
		}
	}

	//=====================================================================================================
	// ускоряемся или замедляемся для достижения заданной скорости и с учётом оставшегося расстояния
	void update_velocity(float targetVelocity, float length)
	{
		int lastState = state;
		//v^2 = 2g*h; //сначала проверяем, что не врежемся с разгона
		if (velocity * velocity > (acceleration * length * 2))
		{
			state = -2;
			//v = sqrt(2*g*h)
			velocity = sqrtf(acceleration * length * 2);
		}
		else if (velocity > targetVelocity)
		{
			state = -1;
			if (lastState != state)
				start_acceleration();
			else
				accelerate(-1);
			if(velocity < 0)
				velocity = 0;
		}
		else if (velocity < targetVelocity)
		{
			state = 1;
			if (lastState != state)
				start_acceleration();
			else
				accelerate(1);
			if(velocity > targetVelocity)
				velocity = targetVelocity;
		}
		else
			state = 0;
	}
};
