#pragma once
#include "winerror.h"
#include "windows.h"
#include <string>
#include <assert.h>
#include <fstream>

class ComPortConnect
{
public:
    ComPortConnect(void); //обнуляет всё
    ~ComPortConnect(void);

    int init_port(int portNumber);                       //подключается к порту
    void send_data(char *buffer, int count);             //шлёт данные
    static DWORD WINAPI receive_thread( LPVOID lpParam );//принимает данные

    HANDLE hThread;    //поток чтения
    HANDLE hCom;       //хэндл порта
    OVERLAPPED ovRead; //переменные для одновременной работы с портом
    OVERLAPPED ovWrite;

    int receiveBPS;    //число прочитанных байт
    int transmitBPS;   //число записанных байт
    int errs;          //ошибки разбиения на пакеты

    static const int RECEIVE_SIZE = 100;
    char receiveBuffer[RECEIVE_SIZE]; //текущий принимаемый пакет
    int receivedSize;                 //принято байт текущего пакета

    std::function<bool(char *data, int size)> on_packet_received;      //устройство, принимающее пакеты

private:
    enum States
    {
		STATES_NORMAL,  //приём пакета
        STATES_CODE,    //принят управляющий символ
    };

    enum Tags
    {
        OP_CODE = '\\', //признак, что дальше идёт управляющий код, если надо послать 100, надо послать его 2 раза
        OP_STOP = 'n',  //конец пакета
    };

    States receiveState;

    void process_bytes(char *buffer, int count);   //формирует пакет из принятых данных
};

