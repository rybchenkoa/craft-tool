#include <QFileDialog>
#include <QAction>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "log.h"


#include "GCodeInterpreter.h"
extern Interpreter::GCodeInterpreter *g_inter;
extern CRemoteDevice *g_device;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->menuOpenProgram, SIGNAL(triggered()), this, SLOT(menu_open_program()));
    connect(ui->c_set0Button, SIGNAL(clicked()), this, SLOT(set0()));
    connect(&updateTimer, SIGNAL(timeout()), this, SLOT(update_state()));

    ui->c_3dView->installEventFilter(this);

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

void MainWindow::set0()
{
    g_inter->runner.csd[0].pos0.x = g_device->currentCoords.r[0];
    g_inter->runner.csd[0].pos0.y = g_device->currentCoords.r[1];
    g_inter->runner.csd[0].pos0.z = g_device->currentCoords.r[2];
}

void MainWindow::update_state()
{
    int currentLine = g_device->get_current_line();
    if(currentLine < ui->c_commandList->count())
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

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    if (object == ui->c_3dView && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        //log_warning("key %d, v %d, s %d\n", ke->key(), ke->nativeVirtualKey(), ke->nativeScanCode());
        switch(ke->nativeVirtualKey())
        {
        case VK_RIGHT:
            g_inter->move(0, 1);
            return true;
        case VK_LEFT:
            g_inter->move(0, -1);
            return true;
        case VK_UP:
            g_inter->move(1, 1);
            return true;
        case VK_DOWN:
            g_inter->move(1, -1);
            return true;
        case Qt::Key_W:
            g_inter->move(2, 1);
            return true;
        case Qt::Key_S:
            g_inter->move(2, -1);
            return true;
        }
    }
    return false;
}

