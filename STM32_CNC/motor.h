//#include "math.h"
#include "common.h"

//===============================================================
//всё что относится к одному экземпляру двигателя
struct Motor
{
	int index;         //номер для доступа к выводам контроллера
	int position;      //по каким координатам сейчас расположена гайка

	//===============================================================
	Motor()
	{
		index = 0;
		position = 0;
	}
	
	//===============================================================
	void reset()
	{
		position = 0;
	}
	
	//===============================================================
	void set_direction(bool direction)
	{
		switch (index)
		{
			case 0:
				set_pin_state<1>(direction);
				break;
			case 1:
				set_pin_state<3>(direction);
				break;
			case 2:
				set_pin_state<5>(direction);
				break;
			case 3:
				set_pin_state<7>(direction);
				break;
		}
	}
	
	//===============================================================
	void set_position(int position)
	{
		bool state = ((position & 1) != 0);
		switch (index)
		{
			case 0:
				set_pin_state<0>(state);
				break;
			case 1:
				set_pin_state<2>(state);
				break;
			case 2:
				set_pin_state<4>(state);
				break;
			case 3:
				set_pin_state<6>(state);
				break;
		}
	}
};

Motor motor[COUNT_DRIVES];
