#pragma once

// разные типы подключений наследуются от этого класса
class BaseConnect
{
public:
	virtual ~BaseConnect() {};

	virtual void send_data(char *buffer, int count) {};
	std::function<void(char *data, int size)> on_bytes_received;

	virtual bool connected() {
		return false;
	};
};

// заворачивает разные способы подключения в общий поток пакетов
class UniversalConnection
{
public:
	void init();
	void send_data(char *buffer, int count);     //заворачивает данные в пакет и шлёт
	void receive_data(char *buffer, int count);  //формирует пакет из принятых данных

	std::unique_ptr<BaseConnect> connect;
	std::function<bool(char *data, int size)> on_packet_received;

	int receiveBPS;    //число прочитанных байт
	int transmitBPS;   //число записанных байт
	int errs;          //ошибки разбиения на пакеты

	static const int RECEIVE_SIZE = 100;
	char receiveBuffer[RECEIVE_SIZE]; //текущий принимаемый пакет
	int receivedSize = 0;             //принято байт текущего пакета

private:
	enum class State
	{
		NORMAL,  //приём пакета
		CODE,    //принят управляющий символ
	};

	enum Tags
	{
		OP_CODE = '\\', //признак, что дальше идёт управляющий код, если надо послать '\', надо его продублировать
		OP_STOP = 'n',  //конец пакета
	};

	State receiveState = State::NORMAL; // состояние парсинга пакета
};
