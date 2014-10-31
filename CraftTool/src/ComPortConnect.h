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
    ComPortConnect(void);
    ~ComPortConnect(void);

    int init_port(int portNumber);
    static DWORD WINAPI send_thread( LPVOID lpParam );
    static DWORD WINAPI receive_thread( LPVOID lpParam );
    void process_bytes(char *buffer, int count);
    void send_data(char *buffer, int count);

    HANDLE hCom;
    OVERLAPPED ovRead;
    OVERLAPPED ovWrite;

    DWORD receiveBPS;
    DWORD transmitBPS;
    int errs;

    static const int RECEIVE_SIZE = 100;
    char receiveBuffer[RECEIVE_SIZE]; //текущий принимаемый пакет
    int receivedSize;

    IPortToDevice *remoteDevice;

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
};

