#include "ComPortConnect.h"
//здесь описана связь по com порту пакетами

int ComPortConnect::init_port(int portNumber)
{
    char portName[30];
    sprintf(portName, "\\\\.\\COM%d", portNumber);

    hCom = CreateFileA(portName, GENERIC_READ | GENERIC_WRITE, 0,
        NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (hCom == INVALID_HANDLE_VALUE)
        throw("cant open port");

    if(!PurgeComm(hCom, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR))
        throw("cant purge port");

    if(!ClearCommBreak(hCom))
        throw("cant run port");

    DCB dcb;
    if (!GetCommState(hCom, &dcb))
        throw("cant read port params");

    dcb.BaudRate = 115200;//9600;//57600;//
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;

    //Setup the flow control
    dcb.fBinary = TRUE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fDtrControl = FALSE; //DTR_CONTROL_HANDSHAKE;
    dcb.fRtsControl = FALSE; //RTS_CONTROL_HANDSHAKE;
    dcb.fNull = FALSE;
    dcb.fAbortOnError = FALSE;

    if (!SetCommState(hCom, &dcb))
        throw("cant set port params");

    DWORD CommEventMask = EV_RXCHAR | EV_RXFLAG | EV_TXEMPTY | EV_ERR | EV_BREAK;
         //| EV_CTS | EV_DSR | EV_RING | EV_RLSD;

    //GetCommMask(hCom,&CommEventMask);
    if (!SetCommMask(hCom,CommEventMask))
        throw("cant set port events");

    COMMTIMEOUTS timeouts;

    if(!GetCommTimeouts(hCom, &timeouts))
        throw("cant read port timeouts");

    timeouts.ReadIntervalTimeout = 5000;
    timeouts.ReadTotalTimeoutConstant = 5000;
    timeouts.ReadTotalTimeoutMultiplier = 5000;
    timeouts.WriteTotalTimeoutConstant = 5000;
    timeouts.WriteTotalTimeoutMultiplier = 5000;

    if(!SetCommTimeouts(hCom, &timeouts))
        throw("cant write port timeouts");

/*
    DWORD   threadId;
    HANDLE hThread = CreateThread( 
            NULL,                   //параметры безопасности
            0,                      //размер стека
            send_thread,            //функция потока
            this,                   //аргумент для этой функции
            0,                      //какие-то флаги создания
            &threadId);             //возвращает ид потока
*/
    DWORD   threadId2;
    HANDLE hThread2 = CreateThread(NULL, 0, receive_thread, this, 0, &threadId2);

    return 0;
}

DWORD WINAPI ComPortConnect::send_thread( LPVOID lpParam )
{
    ComPortConnect *_this = (ComPortConnect*)lpParam;

    DWORD write;
    char data[100];
    _this->ovWrite.hEvent=CreateEvent(NULL, TRUE, TRUE, NULL);

    while(true)
    {
        int result = WriteFile(_this->hCom, data, strlen(data), &write, &_this->ovWrite);

        WaitForSingleObject(_this->ovWrite.hEvent, INFINITE);
        result = GetOverlappedResult(_this->hCom, &_this->ovWrite, &write, TRUE);
        _this->transmitBPS += write-4;
        if(!result)
            printf("error1: %d\n", GetLastError());
    }

    CloseHandle(_this->ovWrite.hEvent);
    return 0;
}

void ComPortConnect::send_data(char *buffer, int count)
{
    char data[1000];
    char *pointer = data;
    *(pointer++) = OP_CODE;
    *(pointer++) = OP_RUN;
    for(int i=0;i<count;i++)
    {
        if(buffer[i] == OP_CODE)
        {
            *(pointer++) = OP_CODE;
            *(pointer++) = OP_CODE;
        }
        else
            *(pointer++) = buffer[i];
    }
    *(pointer++) = OP_CODE;
    *(pointer++) = OP_STOP;
    *(pointer++) = OP_CODE; //второй раз страховочный конец пакета
    *(pointer++) = OP_STOP;

    DWORD write;
    WriteFile(hCom, data, pointer-data, &write, &ovWrite);
}

void ComPortConnect::process_bytes(char *buffer, int count)
{
    for(int i=0;i<count;i++)
    {
        char data = buffer[i];
        //printf("%c", int(data));
        switch (receiveState)
        {
            case S_READY:
                if (data == OP_CODE)
                    receiveState = S_RUN;
                break;

            case S_RUN:
                if (data == OP_RUN)
                {
                    receiveState = S_RECEIVING;
                    receivedSize = 0;
                }
                else
                    receiveState = S_READY;
                break;

            case S_RECEIVING:
            {
                if (data == OP_CODE)
                {
                    receiveState = S_CODE;
                    break;
                }
                else
                    if(receivedSize < RECEIVE_SIZE)
                        receiveBuffer[receivedSize++] = data;
                break;
            }

            case S_CODE:
            {
                switch (data)
                {
                    case OP_CODE:                 //в пересылаемом пакете случайно был байт '\'
                        if(receivedSize < RECEIVE_SIZE)
                            receiveBuffer[receivedSize++] = data;   //и мы его переслали таким образом
                        else
                            receivedSize = 0;
                        receiveState = S_RECEIVING;
                        break;
                    case OP_STOP:
                        receiveState = S_END;
                        remoteDevice->on_packet_received(receiveBuffer, receivedSize); //вот пакет наконец принят
                        receiveState = S_READY;
                        break;
                }
                break;
            }

            case S_END:
                break;

            default:
                break;
        }
    }
}


DWORD WINAPI ComPortConnect::receive_thread( LPVOID lpParam )
{
    ComPortConnect *_this = (ComPortConnect*)lpParam;

    DWORD eventMask = 0;
    DWORD dwReaded = 0;

    const int bufferSize = 10;
    char inData[bufferSize+2];
    memset(inData, 0, sizeof(inData));

    _this->ovRead.hEvent=CreateEvent(NULL, TRUE, TRUE, NULL);

    do
    {
        int retcode = WaitCommEvent(_this->hCom, &eventMask, &_this->ovRead);
        if ( ( !retcode ) && (GetLastError()==ERROR_IO_PENDING) )
        {
            //printf("COM: wait event\n");
            WaitForSingleObject(_this->ovRead.hEvent, 1);
        }

        //printf("COM: event %d\n", eventMask);
        //print_event_mask(eventMask);

        if (eventMask & EV_ERR)
        {
            DWORD ErrorMask = 0; // сюда будет занесен код ошибки порта, если таковая была
            COMSTAT CStat;

            ClearCommError(_this->hCom, &ErrorMask, &CStat);

            //DWORD quelen = CStat.cbInQue; //размер буфера порта
        }
        if (eventMask & EV_RXCHAR)
        {
            //printf("COM: bytes received \n");
            while(true)
            {
                memset(inData, 0, sizeof(inData));
                retcode = ReadFile(_this->hCom, inData, bufferSize, &dwReaded, &_this->ovRead);
//receiveBPS += dwReaded;

                if( retcode == 0 && GetLastError() == ERROR_IO_PENDING ) //не успели прочитать
                {
                    WaitForSingleObject(_this->ovRead.hEvent, INFINITE);
                    retcode = GetOverlappedResult(_this->hCom, &_this->ovRead, &dwReaded, FALSE) ;
                }

_this->receiveBPS += dwReaded;

                if (dwReaded>0) //если прочитали данные
                {
                    //printf("%d байт прочитано: '%s'\n", dwReaded, inData);
                    //printf(inData);
                    _this->process_bytes(inData, dwReaded);
                }
                else
                    break;
                    //printf("не удалось прочитать, ошибка %d\n", GetLastError());
                //break;
            }
        }
        if (eventMask & EV_TXEMPTY)
        {
            //printf("COM: bytes sended\n");
        }


        eventMask=0;
        ResetEvent(_this->ovRead.hEvent);
    }
    while(true);

    BOOL bSuccess = CloseHandle(_this->hCom);
    _this->hCom = INVALID_HANDLE_VALUE;

    return 0;
}


ComPortConnect::ComPortConnect(void)
{
    memset(&ovRead,0,sizeof(ovRead));
    memset(&ovWrite,0,sizeof(ovWrite));
    receiveState = S_READY;
}


ComPortConnect::~ComPortConnect(void)
{
}
