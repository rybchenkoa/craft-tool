#include <QApplication>
#include <QDir>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "log.h"
#include "config_defines.h"
#include "GCodeInterpreter.h"
#include "RemoteDevice.h"

QApplication *g_application = 0;
std::string appDir;


static DWORD WINAPI execute( LPVOID lpParam )
{
    Q_UNUSED(lpParam)

    RemoteDevice *remoteDevice = new RemoteDevice; //управление удалённым устройством

    QObject::connect(remoteDevice, SIGNAL(coords_changed(float, float, float)),
                     g_mainWindow->ui->c_3dView, SLOT(update_tool_coords(float, float, float)));

	UniversalConnection *connection = new UniversalConnection; //устройство доводит данные до реального устройтва через подключение

    remoteDevice->connection = connection; //говорим устройству, через что слать
    connection->on_packet_received = std::bind(&RemoteDevice::on_packet_received, remoteDevice, std::placeholders::_1, std::placeholders::_2); //порту говорим, кто принимает

    g_device = remoteDevice;

    try
    {
        connection->init(); //открываем порт
    }
    catch(std::string message)
    {
        log_warning("%s\n", message.c_str());
    }

    g_inter->remoteDevice = remoteDevice;

	try
    {
		g_device->init();
		g_inter->init();
	}
	catch(std::string message)
    {
        log_warning("%s\n", message.c_str());
    }

    return 0;
}

int main(int argc, char* argv[])
{
    appDir = QDir::current().absolutePath().toStdString();
    g_application = new QApplication(argc, argv);
    g_config = new Config();
    if (!g_config->read_from_file(CFG_CONFIG_NAME))
        log_warning("config not found");
    g_inter = new Interpreter::GCodeInterpreter();
    g_mainWindow = new MainWindow();
    QObject::connect(&g_logger, SIGNAL(send_string(QColor, QString)),
                     g_mainWindow->ui->c_logConsole, SLOT(output(QColor, QString)));
    g_mainWindow->show();

    execute(0);

    return g_application->exec();
}
