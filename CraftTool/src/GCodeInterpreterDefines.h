#pragma once

namespace Interpreter
{
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
