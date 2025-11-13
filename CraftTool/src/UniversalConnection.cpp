#include "UniversalConnection.h"
#include "ComPortConnect.h"
#include "log.h"
#include "config_defines.h"
// оборачивает паки разметкой


void UniversalConnection::init()
{
	receiveBPS = 0;
	transmitBPS = 0;
	errs = 0;

	std::vector<std::string> params;
	g_config->get_array(CFG_CONNECTION, params);

	auto defaultConnection = new BaseConnect(); // заглушка подключения
	connect.reset(defaultConnection);

	if (params.size() > 1) {
		if (params[0] == "com") // com portNumber baudRate
		{
			if (params.size() != 3)
				throw std::string("invalid connection format, need 'com portNumber baudrate'");

			int portNumber = std::stoi(params[1]);
			int baudRate = std::stoi(params[2]);

			auto port = new ComPortConnect();
			connect.reset(port);
			connect->on_bytes_received = [this](char* data, int size) { receive_data(data, size); };
			port->init(portNumber, baudRate);
			throw port->get_state();
		}
	}

	if (connect.get() == defaultConnection)
		throw std::string("invalid connection type in config");
}


void UniversalConnection::send_data(char *buffer, int count)
{
	if (!connect->connected())
		return;

	char data[1000];
	char *pointer = data;
	for (int i = 0; i < count; i++)
	{
		if (buffer[i] == OP_CODE)
		{
			*(pointer++) = OP_CODE;
			*(pointer++) = OP_CODE;
		}
		else
			*(pointer++) = buffer[i];
	}
	*(pointer++) = OP_CODE;
	*(pointer++) = OP_STOP;

	connect->send_data(data, pointer - data);
	transmitBPS += pointer - data;
}


void UniversalConnection::receive_data(char *buffer, int count)
{
	receiveBPS += count;

	for (int i = 0; i<count; i++)
	{
		char data = buffer[i];
		//log_message("%c", int(data));
		/*
		extern std::string appDir;
		extern int get_timestamp();
		std::ofstream f;
		f.open(appDir +  "/message2.log", std::ios::app);
		f << int(data) << " ";
		if (data == OP_STOP)
		{
		int ts = get_timestamp();
		f << "\n[" << ts << "] ";
		}
		*/
		switch (receiveState)
		{
			case State::NORMAL:
			{
				if (data == OP_CODE)
					receiveState = State::CODE;
				else
				{
					if (receivedSize < RECEIVE_SIZE)
						receiveBuffer[receivedSize++] = data;
					else
					{
						receivedSize = 0;
						++errs;
					}
				}
				break;
			}

			case State::CODE:
			{
				receiveState = State::NORMAL;
				switch (data)
				{
				case OP_CODE:                 //в пересылаемом пакете случайно был байт '\'
					if (receivedSize < RECEIVE_SIZE)
						receiveBuffer[receivedSize++] = data;   //и мы его переслали таким образом
					else
					{
						receivedSize = 0;
						++errs;
					}
					break;

				case OP_STOP:
					on_packet_received(receiveBuffer, receivedSize); //пакет наконец принят
					receivedSize = 0;
					break;

				default:
					receivedSize = 0;
					++errs;
				}
				break;
			}
		}
	}
}
