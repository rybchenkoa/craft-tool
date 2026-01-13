#include "GCodeLexer.h"
#include "log.h"
#include "config_defines.h"

using namespace Interpreter;

//====================================================================================================
//читает данные из строки в массив
InterError GCodeLexer::parse_codes(const char *frame)
{
	codes.clear();
	position = 0;
	string = frame;
	state = InterError();

	GKey current;
	current.position = position;
	while(parse_code(current.letter, current.value))
	{
		codes.push_back(current);
		current.position = position;
	}

	return state;
}

//====================================================================================================
//читает следующий код
bool GCodeLexer::parse_code(char &letter, double &value)
{
	find_significal_symbol();
	if (string[position] == 0)
		return false;

	letter = string[position];

	if(letter == '%')
		return false;

	if(letter >= 'a' && letter <= 'z')
		letter += 'A' - 'a';

	if(letter < 'A' || letter > 'Z')
	{
		state = InterError(InterError::WRONG_LETTER, std::string("wrong letter: ") + letter);
		return false;
	}

	position++;

	find_significal_symbol();
	if (!parse_value(value))
	{
		state = InterError(InterError::WRONG_VALUE, std::string("cant parse value"));
		return false;
	};

	return true;
}

//====================================================================================================
//пропускает пробелы
void GCodeLexer::accept_whitespace()
{
	while (string[position] == ' ' || string[position] == '\t') position++;
}

//====================================================================================================
//доходит до следующего кода
void GCodeLexer::find_significal_symbol()
{
	while(string[position] != 0)
	{
		accept_whitespace();
		if(string[position] == '(')
		{
			while(string[position] != ')' && string[position] != 0) position++;
			if(string[position] == ')') position++;
		}
		else
			break;
	}
}

//====================================================================================================
//читает число
bool GCodeLexer::parse_value(double &dst)
{
	const char *cursor = string + position;

	double value = 0;
	int sign = 1;       // +-
	int numDigits = 0;  //сколько цифр прочитано
	int maxDigits = 20; //сколько всего можно
	double denominator = 1;//на сколько поделить прочитанное

	if (*cursor == '-')
		sign = -1;
	else if (*cursor == '+')
		sign = 1;
	else if(*cursor >= '0' && *cursor <= '9')
	{
		value = *cursor - '0';
		numDigits++;
	}
	else if(*cursor != '.')
		return false;

	if(*cursor != '.')
		++cursor;

	while(*cursor >= '0' && *cursor <= '9' && ++numDigits <= maxDigits)
		value = value*10 + (*(cursor++) - '0');

	if(numDigits > maxDigits)
	{
		position = cursor - string;
		return false;
	}

	if(*cursor == '.')
	{
		++cursor;
		while(*cursor >= '0' && *cursor <= '9' && ++numDigits <= maxDigits)
		{
			value = value*10 + (*(cursor++) - '0');
			denominator *= 10;
		}

		if(numDigits > maxDigits)
		{
			position = cursor - string;
			return false;
		}

		value /= denominator;
	}

	dst = value * sign;
	position = cursor - string;
	return true;
}
