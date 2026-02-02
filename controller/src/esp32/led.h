#pragma once
// мигание светодиодом

static const int LED_PINS[] = {2, 2};
struct Led
{
	bool on[2];

	Led()
	{
		on[0] = false;
		on[1] = false;
	}

	void init()
	{
		pinMode(LED_PINS[0], OUTPUT);
		pinMode(LED_PINS[1], OUTPUT);
	}

	void flip(int i)
	{
		if(on[i])
			hide(i);
		else
			show(i);
	}

	void show(int i)
	{
		digitalWrite(LED_PINS[i], HIGH);
		on[i] = true;
	}

	void hide(int i)
	{
		digitalWrite(LED_PINS[i], LOW);
		on[i] = false;
	}
};

Led led;
