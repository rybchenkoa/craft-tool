#pragma once
// исполняемая команда

class Task
{
public:

	enum OperateResult
	{
		END = 0,
		WAIT,
	};

	virtual OperateResult update(bool needBreak); // = 0
};
