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
    load_file(str.toLocal8Bit().data());
}

void MainWindow::on_c_refHomeButton_clicked()
{
    //g_inter->runner.csd[0].pos0.x = g_device->currentCoords.r[0];
    //g_inter->runner.csd[0].pos0.y = g_device->currentCoords.r[1];
    //g_inter->runner.csd[0].pos0.z = g_device->currentCoords.r[2];
	g_device->homing();
}

void MainWindow::on_c_setX0Button_clicked()
{
    g_inter->runner.csd[0].pos0.x = g_device->currentCoords.r[0];
}

void MainWindow::on_c_setY0Button_clicked()
{
    g_inter->runner.csd[0].pos0.y = g_device->currentCoords.r[1];
}

void MainWindow::on_c_setZ0Button_clicked()
{
    g_inter->runner.csd[0].pos0.z = g_device->currentCoords.r[2];
}

void MainWindow::on_c_toZeroButton_clicked()
{
    g_inter->runner.position = g_inter->runner.csd[0].pos0;
    g_device->set_position(g_inter->runner.position);
}

void MainWindow::on_c_feedSlider_valueChanged(int value)
{
    g_device->set_feed_multiplier(value / 100.0);
}

void MainWindow::update_state()
{
    int currentLine = g_device->get_current_line();
    if(currentLine < ui->c_commandList->count())
        ui->c_commandList->setCurrentRow(currentLine);

    Coords coords = g_device->currentCoords;
	if (!ui->c_machineCoordsButton->isChecked())
		for (int i = 0; i < MAX_AXES; ++i)
			coords.r[i] -= g_inter->runner.csd[0].pos0.r[i];
    ui->c_lineEditX->setText(QString::number(coords.x));
    ui->c_lineEditY->setText(QString::number(coords.y));
    ui->c_lineEditZ->setText(QString::number(coords.z));
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

void MainWindow::on_c_pauseButton_clicked()
{
    g_device->pause_moving(ui->c_pauseButton->isChecked());
}

void MainWindow::on_c_startButton_clicked()
{
    g_inter->execute_file(nullptr);//запускаем интерпретацию
}

void MainWindow::on_c_runLineButton_clicked()
{
    g_inter->execute_line(ui->c_lineEditCommand->text().toStdString());//запускаем интерпретацию
}

void MainWindow::on_c_refreshTrajectory_clicked()
{
    Interpreter::Trajectory trajectory;
    g_inter->execute_file(&trajectory); //формируем траекторию
    ui->c_3dView->track.clear();
    ui->c_3dView->track.reserve(trajectory.size());
    for (unsigned i = 0; i < trajectory.size(); ++i)
    {
        TrackPoint point;
        point.isFast = trajectory[i].isFast;
        point.position = glm::vec3(trajectory[i].position.x, trajectory[i].position.y, trajectory[i].position.z);
        ui->c_3dView->track.push_back(point);
    }
    ui->c_3dView->realTrack.clear();
}

void MainWindow::on_c_topViewButton_clicked()
{
	ui->c_3dView->set_view(View::TOP);
}

void MainWindow::on_c_bottomViewButton_clicked()
{
	ui->c_3dView->set_view(View::BOTTOM);
}

void MainWindow::on_c_frontViewButton_clicked()
{
	ui->c_3dView->set_view(View::FRONT);
}

void MainWindow::on_c_backViewButton_clicked()
{
	ui->c_3dView->set_view(View::BACK);
}

void MainWindow::on_c_leftViewButton_clicked()
{
	ui->c_3dView->set_view(View::LEFT);
}

void MainWindow::on_c_rightViewButton_clicked()
{
	ui->c_3dView->set_view(View::RIGHT);
}

void MainWindow::load_file(char *fileName)
{
    g_inter->read_file(fileName); //читаем данные из файла

    ui->c_commandList->clear();
    for(auto i = g_inter->inputFile.begin(); i != g_inter->inputFile.end(); ++i)
        ui->c_commandList->addItem(i->c_str());

    on_c_refreshTrajectory_clicked();
    setWindowTitle(QString(fileName));
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    if (object == ui->c_3dView && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        //log_warning("key %d, v %d, s %d\n", ke->key(), ke->nativeVirtualKey(), ke->nativeScanCode());
		coord step = 0.1;
        switch(ke->nativeVirtualKey())
        {
        case VK_RIGHT:
            g_inter->move(0, step);
            return true;
        case VK_LEFT:
            g_inter->move(0, -step);
            return true;
        case VK_UP:
            g_inter->move(1, step);
            return true;
        case VK_DOWN:
            g_inter->move(1, -step);
            return true;
        case Qt::Key_W:
            g_inter->move(2, step);
            return true;
        case Qt::Key_S:
            g_inter->move(2, -step);
            return true;
        }
    }
    return false;
}

