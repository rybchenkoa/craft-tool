#pragma once
// команда ждать заданное число миллисекунд

#include "task.h"
#include "sys_timer.h"


class TaskWait : public Task
{
public:

	int stopTime;  // время окончания остановки
	
	OperateResult update(bool needBreak) // override
	{
		if (needBreak)
			return END;

		if(timer.check(stopTime))
			return WAIT;
		else
			return END;
	}

	void init(int delay)
	{
		stopTime = timer.get_mks(delay);
	}
};
