#pragma once
#include "winerror.h"
#include "windows.h"
#include <string>
#include <assert.h>
#include <fstream>

#include "IPortToDevice.h"

class ComPortConnect
{
public:
    ComPortConnect(void); //обнул€ет всЄ
    ~ComPortConnect(void);

    int init_port(int portNumber);                       //подключаетс€ к порту
    void send_data(char *buffer, int count);             //шлЄт данные
    static DWORD WINAPI receive_thread( LPVOID lpParam );//принимает данные

    HANDLE hThread;    //поток чтени€
    HANDLE hCom;       //хэндл порта
    OVERLAPPED ovRead; //переменные дл€ одновременной работы с портом
    OVERLAPPED ovWrite;

    int receiveBPS;    //число прочитанных байт
    int transmitBPS;   //число записанных байт
    int errs;          //ошибки разбиени€ на пакеты

    static const int RECEIVE_SIZE = 100;
    char receiveBuffer[RECEIVE_SIZE]; //текущий принимаемый пакет
    int receivedSize;                 //прин€то байт текущего пакета

    IPortToDevice *remoteDevice;      //устройство, принимающее пакеты

private:
    enum States
    {
        S_READY,     //очередь пуста, никто ничего не присылал
        S_CODE,      //прин€т управл€ющий символ
        S_RUN,       //вначале прин€т символ '\' , ждЄм символ 'r'
        S_RECEIVING, //приЄм пакета
        S_END,       //прин€т символ конца, проверка пакета
    };

    enum Tags
    {
        OP_CODE = '\\', //признак, что дальше идЄт управл€ющий код, если надо послать 100, надо послать его 2 раза
        OP_STOP = 'n',  //конец пакета
        OP_RUN  = 'r',  //начало пакета
    };

    States receiveState;

    void process_bytes(char *buffer, int count);   //формирует пакет из прин€тых данных
};

