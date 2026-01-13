#pragma once

namespace Interpreter
{
	enum class UnitSystem //система единиц
	{
		NONE = 0,
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
		ADC,        // управление подачей через напряжение, G94.2
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
}
