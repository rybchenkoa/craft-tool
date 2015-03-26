#include <QFileDialog>
#include "mainwindow.h"
#include "ui_mainwindow.h"


#include "GCodeInterpreter.h"
extern Interpreter::GCodeInterpreter *g_inter;
extern CRemoteDevice *g_device;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->menuOpenProgram, SIGNAL(triggered()), this, SLOT(menu_open_program()));
    connect(&updateTimer, SIGNAL(timeout()), this, SLOT(update_state()));

    updateTimer.start(100); //10 fps
    time.start();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::menu_open_program()
{
    QString str = QFileDialog::getOpenFileName(0, "Open Dialog", "", "*.*");
    run_file(str.toLocal8Bit().data());
}

void MainWindow::update_state()
{
    int currentLine = g_device->get_current_line();
    ui->c_commandList->setCurrentRow(currentLine);
}

bool MainWindow::connect_to_device()
{
    /*CRemoteDevice *remoteDevice = new CRemoteDevice; //управление удалённым устройством

    QObject::connect(remoteDevice, SIGNAL(coords_changed(float, float, float)),
                     g_mainWindow->ui->c_3dView, SLOT(update_tool_coords(float, float, float)));

    ComPortConnect *comPort = new ComPortConnect;    //устройство доводит данные до реального устройтва через порт

    remoteDevice->comPort = comPort; //говорим устройству, через что слать
    comPort->remoteDevice = remoteDevice; //порту говорим, кто принимает

    try
    {
        int port = 1;
        g_config.get_int(CFG_COM_PORT_NUMBER, port);
        comPort->init_port(port);           //открываем порт
    }
    catch(const char *message)
    {
        qWarning(message);
        log_warning(message);
        return false;
    }*/

    return true;
}

void MainWindow::run_file(char *fileName)
{
    g_inter->read_file(fileName); //читаем данные из файла

    ui->c_commandList->clear();
    for(auto i = g_inter->inputFile.begin(); i != g_inter->inputFile.end(); ++i)
        ui->c_commandList->addItem(i->c_str());

    g_inter->execute_file();           //запускаем интерпретацию
}
