#include <QApplication>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "log.h"

#include "GCodeInterpreter.h"
#include "IRemoteDevice.h"

QApplication *g_application = 0;
MainWindow *g_mainWindow = 0;
Interpreter::GCodeInterpreter g_inter;



static DWORD WINAPI execute( LPVOID lpParam )
{
    Q_UNUSED(lpParam)

    CRemoteDevice *remoteDevice = new CRemoteDevice; //он передаёт команды классу связи с устройством
    ComPortConnect *comPort = new ComPortConnect;    //устройство доводит данные до реального устройтва через порт
    try
    {
        comPort->init_port(3);           //открываем порт (+ всё остальное)
    }
    catch(const char *message)
    {
        qWarning(message);
        log_warning(message);
        return 0;
        exit(1);
    }

    QObject::connect(remoteDevice, SIGNAL(coords_changed(float, float, float)),
                     g_mainWindow->ui->c_3dView, SLOT(update_tool_coords(float, float, float)));

    remoteDevice->comPort = comPort; //говорим устройству, через что слать
    comPort->remoteDevice = remoteDevice; //порту говорим, кто принимает
    g_inter.remoteDevice = remoteDevice;

    g_inter.read_file("..\\..\\test.nc"); //читаем данные из файла
    g_inter.execute_file();           //запускаем интерпретацию

    while(true || !remoteDevice->queue_empty())
    {
        //printf("missed %d\n", remoteDevice->missedSends);
        Sleep(1000);
        char text[500];
        sprintf(text, "%d, %d, %d",
                remoteDevice->missedHalfSend,
                remoteDevice->missedSends,
                remoteDevice->missedReceives);
        //g_mainWindow->ui->c_statusBar->showMessage(text);
    }
}

int main(int argc, char* argv[])
{
    g_application = new QApplication(argc, argv);
    g_mainWindow = new MainWindow();
    QObject::connect(&g_logger, SIGNAL(send_string(QColor, QString)),
                     g_mainWindow->ui->c_logConsole, SLOT(output(QColor, QString)));
    g_mainWindow->show();

    CreateThread(NULL, 0, execute, 0, 0, 0);

    return g_application->exec();
}
