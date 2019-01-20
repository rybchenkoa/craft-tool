#include <QApplication>
#include <QDir>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "log.h"
#include "config_defines.h"
#include "GCodeInterpreter.h"
#include "IRemoteDevice.h"

QApplication *g_application = 0;
MainWindow *g_mainWindow = 0;
Interpreter::GCodeInterpreter *g_inter = 0;
Config *g_config = 0;
CRemoteDevice *g_device = 0;
std::string appDir;


static DWORD WINAPI execute( LPVOID lpParam )
{
    Q_UNUSED(lpParam)

    CRemoteDevice *remoteDevice = new CRemoteDevice; //управление удалённым устройством

    QObject::connect(remoteDevice, SIGNAL(coords_changed(float, float, float)),
                     g_mainWindow->ui->c_3dView, SLOT(update_tool_coords(float, float, float)));

    ComPortConnect *comPort = new ComPortConnect;    //устройство доводит данные до реального устройтва через порт

    remoteDevice->comPort = comPort; //говорим устройству, через что слать
    comPort->on_packet_received = std::bind(&CRemoteDevice::on_packet_received, remoteDevice, std::placeholders::_1, std::placeholders::_2); //порту говорим, кто принимает

    g_device = remoteDevice;

    try
    {
        int port = 1;
        g_config->get_int(CFG_COM_PORT_NUMBER, port);
        comPort->init_port(port);           //открываем порт
    }
    catch(const char *message)
    {
        qWarning(message);
        log_warning(message);
        //return 0;
        //exit(1);
    }

    g_inter->remoteDevice = remoteDevice;

	try
    {
		g_device->init();
		g_inter->init();
	}
	catch(std::string message)
    {
        qWarning(message.c_str());
        log_warning(message.c_str());
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
