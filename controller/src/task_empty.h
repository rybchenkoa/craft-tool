#pragma once
// команда ничего не делать

#include "task.h"
#include "receiver.h"


class TaskEmpty : public Task
{
public:

	OperateResult update(bool needBreak) // override
	{
		if ((unsigned int)timer.get() % 1200000 > 600000)
			led.show(0);
		else
			led.hide(0);

		if (needBreak)
			return END;

		if (receiver.queue_empty())
			return WAIT;
		else
			return END;
	}
};
