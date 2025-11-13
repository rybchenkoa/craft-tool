#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "windows.h"
#include "UniversalConnection.h"


class ComPortConnect: public BaseConnect
{
public:
    ComPortConnect(void); //обнуляет всё
    ~ComPortConnect(void);

    void init(int portNumber, int baudRate);          //подключается к порту
	void open_port();
	bool connected() override;
	std::string get_state() override;
    void send_data(char *buffer, int count) override; //шлёт данные
    void receive_data();            //принимает данные

	int portNumber;
	int baudRate;

    std::thread receiveThread;            //поток чтения
    bool stop_token = false;

    HANDLE hCom = INVALID_HANDLE_VALUE;   //хэндл порта
    OVERLAPPED ovRead; //переменные для одновременной работы с портом
    OVERLAPPED ovWrite;

	std::string stateDescription;
	std::mutex stateMutex;
};
