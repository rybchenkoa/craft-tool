#include "mainwindow.h"
#include <QApplication>

#include "GCodeInterpreter.h"
#include "IRemoteDevice.h"

void execute()
{

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

    execute();

    return a.exec();
}
