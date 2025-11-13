#include "ComPortConnect.h"
#include "log.h"
#include "config_defines.h"
//здесь описана связь по com порту пакетами

void ComPortConnect::open_port()
{
	std::unique_lock<std::mutex> lock(stateMutex);

	char portName[30];
	sprintf(portName, "\\\\.\\COM%d", portNumber);

	hCom = CreateFileA(portName, GENERIC_READ | GENERIC_WRITE, 0,
		NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (hCom == INVALID_HANDLE_VALUE) {
		stateDescription = "cant open port";
		return;
	}

	if (!PurgeComm(hCom, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR)) {
		stateDescription = "cant purge port";
		return;
	}

	if (!ClearCommBreak(hCom)) {
		stateDescription = "cant run port";
		return;
	}

	DCB dcb;
	if (!GetCommState(hCom, &dcb)) {
		stateDescription = "cant read port params";
		return;
	}

	dcb.BaudRate = baudRate;
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

	if (!SetCommState(hCom, &dcb)) {
		stateDescription = "cant set port params";
		return;
	}

	DWORD CommEventMask = EV_RXCHAR | EV_RXFLAG | EV_TXEMPTY | EV_ERR | EV_BREAK;
	//| EV_CTS | EV_DSR | EV_RING | EV_RLSD;

	//GetCommMask(hCom,&CommEventMask);
	if (!SetCommMask(hCom, CommEventMask)) {
		stateDescription = "cant set port events";
		return;
	}

	COMMTIMEOUTS timeouts;

	if (!GetCommTimeouts(hCom, &timeouts)) {
		stateDescription = "cant read port timeouts";
		return;
	}

	timeouts.ReadIntervalTimeout = 5000;
	timeouts.ReadTotalTimeoutConstant = 5000;
	timeouts.ReadTotalTimeoutMultiplier = 5000;
	timeouts.WriteTotalTimeoutConstant = 5000;
	timeouts.WriteTotalTimeoutMultiplier = 5000;

	if (!SetCommTimeouts(hCom, &timeouts)) {
		stateDescription = "cant write port timeouts";
		return;
	}

	stateDescription = "connected";
}


void ComPortConnect::init(int portNumber, int baudRate)
{
	this->portNumber = portNumber;
	this->baudRate = baudRate;

	open_port();
	std::string result = stateDescription;
	receiveThread = std::thread(&ComPortConnect::receive_data, this);
}


bool ComPortConnect::connected()
{
	return hCom != INVALID_HANDLE_VALUE;
}

std::string ComPortConnect::get_state()
{
	std::unique_lock<std::mutex> lock(stateMutex);
	return stateDescription;
}


void ComPortConnect::send_data(char *buffer, int count)
{
    DWORD write;
	BOOL result = WriteFile(hCom, buffer, count, &write, &ovWrite);
    if (!result)
	{
		auto err = GetLastError();
		if (err != ERROR_IO_PENDING)
			log_message("port error");
		else
		{
			result = GetOverlappedResult(hCom, &ovWrite, &write, true);
			if (!result)
				err = GetLastError();
		}
	}
}


void ComPortConnect::receive_data()
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    DWORD eventMask = 0;
    DWORD readed = 0;

    const int bufferSize = 10;
    char inData[bufferSize+2];
    memset(inData, 0, sizeof(inData));

    do
    {
        int retcode = WaitCommEvent(hCom, &eventMask, &ovRead);

		// если порт не инициализирован или отключен, переинициализируем
		auto error = GetLastError();
		if (error == ERROR_INVALID_HANDLE || error == ERROR_ACCESS_DENIED) {
			CloseHandle(hCom);
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			open_port();
			continue;
		}

        if ( retcode = 0 && GetLastError() == ERROR_IO_PENDING )
        {
            //printf("COM: wait event\n");
            WaitForSingleObject(ovRead.hEvent, 1);
        }

        //printf("COM: event %d\n", eventMask);

        if (eventMask & EV_ERR)
        {
            DWORD ErrorMask = 0; // сюда будет занесен код ошибки порта, если таковая была
            COMSTAT CStat;

            ClearCommError(hCom, &ErrorMask, &CStat);

            //DWORD quelen = CStat.cbInQue; //размер буфера порта
        }
        if (eventMask & EV_RXCHAR)
        {
            //printf("COM: bytes received \n");
            while(true)
            {
                memset(inData, 0, sizeof(inData));
                retcode = ReadFile(hCom, inData, bufferSize, &readed, &ovRead);

                if( retcode == 0 && GetLastError() == ERROR_IO_PENDING ) //не успели прочитать
                {
                    WaitForSingleObject(ovRead.hEvent, INFINITE);
                    retcode = GetOverlappedResult(hCom, &ovRead, &readed, FALSE) ;
                }

                if (readed > 0) //если прочитали данные
                {
                    //printf("%d байт прочитано: '%s'\n", readed, inData);
                    //printf(inData);
					on_bytes_received(inData, readed);
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
        ResetEvent(ovRead.hEvent);
    }
    while(!stop_token);

    CloseHandle(hCom);
    hCom = INVALID_HANDLE_VALUE;
}


ComPortConnect::ComPortConnect(void)
{
    memset(&ovRead,0,sizeof(ovRead));
    memset(&ovWrite,0,sizeof(ovWrite));
	ovRead.hEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
	ovWrite.hEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
}


ComPortConnect::~ComPortConnect(void)
{
    CloseHandle(hCom);
    stop_token = true;
    receiveThread.join();
}
