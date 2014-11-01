#include <QApplication>
#include "mainwindow.h"

#include "GCodeInterpreter.h"
#include "IRemoteDevice.h"

static DWORD WINAPI execute( LPVOID lpParam )
{
    Q_UNUSED(lpParam)

    Interpreter::GCodeInterpreter inter;             //есть у нас интерпретатор
    CRemoteDevice *remoteDevice = new CRemoteDevice; //он передаёт команды классу связи с устройством
    ComPortConnect *comPort = new ComPortConnect;    //устройство доводит данные до реального устройтва через порт
    try
    {
        comPort->init_port(3);           //открываем порт (+ всё остальное)
    }
    catch(const char *a)
    {
        qWarning(a);
        exit(1);
    }
    remoteDevice->comPort = comPort; //говорим устройству, через что слать
    comPort->remoteDevice = remoteDevice; //порту говорим, кто принимает
    inter.remoteDevice = remoteDevice;

    inter.read_file("..\\..\\test.nc"); //читаем данные из файла
    inter.execute_file();           //запускаем интерпретацию

    while(true || !remoteDevice->queue_empty())
    {
        //printf("missed %d\n", remoteDevice->missedSends);
        Sleep(1000);
    }
}

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    CreateThread(NULL, 0, execute, 0, 0, 0);

    return a.exec();
}
