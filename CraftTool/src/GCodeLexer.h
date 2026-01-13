#pragma once
#include "GCodeInterpreterDefines.h"

//превращает строку текста в список токенов G-кода
namespace Interpreter
{
	struct GKey
	{
		char letter;
		double value;
		int position;
	};

	//здесь переменные для чтения команд
	struct GCodeLexer
	{
		const char *string;
		int  position;
		InterError  state;
		std::vector<GKey> codes;

		InterError parse_codes(const char *frame); //читает коды и значения параметров

		bool parse_code(char &letter, double &value); //следующий код
		void accept_whitespace(); //пропускает пробелы
		void find_significal_symbol(); //пропускает комментарии, пробелы
		bool parse_value(double &value); //считывает число
	};
}
