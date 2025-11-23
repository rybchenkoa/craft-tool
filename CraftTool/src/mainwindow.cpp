#include <QFileDialog>
#include <QAction>
#include <QMimeData>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "log.h"
#include "config_defines.h"
#include "GCodeInterpreter.h"

extern Interpreter::GCodeInterpreter *g_inter;
extern CRemoteDevice *g_device;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
	std::string stylePath;
	if (g_config->get_string(CFG_STYLESHEET, stylePath))
	{
		QFile styleFile;
		styleFile.setFileName(stylePath.c_str());
		if (styleFile.open(QFile::ReadOnly))
		{
			QString style = styleFile.readAll();
			setStyleSheet(style);
		}
	}
    ui->setupUi(this);

	std::string axes = "XYZAB";
	for(int i = 0; i < axes.size(); ++i) {
		buttonSet0.push_back(findChild<QAbstractButton*>(QString("ButtonSet0") + axes[i]));
		textCoord.push_back(findChild<QLineEdit*>(QString("c_lineEdit") + axes[i]));
		buttonPlusCoord.push_back(findChild<QAbstractButton*>(QString("ButtonPlus") + axes[i]));
		buttonMinusCoord.push_back(findChild<QAbstractButton*>(QString("ButtonMinus") + axes[i]));
	}

	const char* views[] = {"Top", "Bottom", "Left", "Right", "Front", "Back"};
	for(int i = 0; i < 6; ++i) {
		auto button = findChild<QAbstractButton*>(QString("ButtonView") + views[i]);
		connect(button, &QAbstractButton::clicked, [i, this]() {
			ui->c_3dView->set_view(View(i));
		});
	}

	coord stp[] = {1, 0.5, 0.1, 0.05, 0.01, 0.001}; //TODO сделать чтение из конфига
	steps.assign(stp, stp+6);
	for(int i = 0; i < steps.size(); ++i){
		ui->c_stepSwitch->insertItem(i, QString::number(steps[i]));
	}
	ui->c_stepSwitch->setCurrentIndex(2);

	for(int i = 0; i < axes.size(); ++i) {
		connect(buttonSet0[i], &QAbstractButton::clicked, [i]() {
			g_inter->runner.csd[0].pos0.r[i] = g_device->currentCoords.r[i];
		});

		connect(buttonPlusCoord[i], &QAbstractButton::clicked, [this, i]() {
			manual_move(i, 1, false);
		});

		connect(buttonMinusCoord[i], &QAbstractButton::clicked, [this, i]() {
			manual_move(i, -1, false);
		});
	}

    connect(ui->menuOpenProgram, SIGNAL(triggered()), this, SLOT(menu_open_program()));
    connect(ui->menuCloseProgram, SIGNAL(triggered()), this, SLOT(menu_close_program()));
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
	if (!str.isEmpty()) {
		load_file(str);
	}
}

void MainWindow::menu_close_program()
{
	load_file("");
}

void MainWindow::dragEnterEvent( QDragEnterEvent* event )
{
	if (event->mimeData()->hasUrls()) {
		event->acceptProposedAction();
	}
}

void MainWindow::dropEvent( QDropEvent* event )
{
	QString fileName = event->mimeData()->urls()[0].toLocalFile();
	load_file(event->mimeData()->urls()[0].toLocalFile());
	event->acceptProposedAction();
}

void MainWindow::on_c_refHomeButton_clicked()
{
    //g_inter->runner.csd[0].pos0.x = g_device->currentCoords.r[0];
    //g_inter->runner.csd[0].pos0.y = g_device->currentCoords.r[1];
    //g_inter->runner.csd[0].pos0.z = g_device->currentCoords.r[2];
	g_device->homing();
}

void MainWindow::on_ButtonToX0Y0_clicked()
{
	g_inter->execute_line("G0 X0 Y0");
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
		coords -= g_inter->runner.csd[0].pos0;
	for (int i = 0; i < MAX_AXES; ++i)
		textCoord[i]->setText(QString::number(coords.r[i]));
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

void MainWindow::on_c_breakButton_clicked()
{
    g_device->break_queue();
    g_inter->coordsInited = false;
    ui->c_pauseButton->setChecked(false);
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
    for (const auto& tPoint: trajectory)
    {
        TrackPoint point;
        point.isFast = tPoint.isFast;
        point.position = glm::vec3(tPoint.position.x, tPoint.position.y, tPoint.position.z);
        ui->c_3dView->track.push_back(point);
    }
    ui->c_3dView->realTrack.clear();
}

void MainWindow::load_file(QString fileName)
{
    g_inter->read_file(fileName.toLocal8Bit().data()); //читаем данные из файла

    ui->c_commandList->clear();
	int index = 1;
    for(const auto& line : g_inter->inputFile)
        ui->c_commandList->addItem((to_string(index++) + ": " + line).c_str());

    on_c_refreshTrajectory_clicked();
    setWindowTitle(fileName);
}

void MainWindow::manual_move(int axe, int dir, bool fast)
{
	coord step = steps[ui->c_stepSwitch->currentIndex()];
	g_inter->move(axe, step * dir, fast);
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    if (object == ui->c_3dView && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        //log_warning("key %d, v %d, s %d\n", ke->key(), ke->nativeVirtualKey(), ke->nativeScanCode());
		coord step = ke->modifiers().testFlag(Qt::ControlModifier)? 10:1;
        bool fast = ke->modifiers().testFlag(Qt::ShiftModifier);

        switch(ke->nativeVirtualKey())
        {
        case VK_RIGHT:
			manual_move(0, step, fast);
            return true;
        case VK_LEFT:
			manual_move(0, -step, fast);
            return true;

        case VK_UP:
            manual_move(1, step, fast);
            return true;
        case VK_DOWN:
            manual_move(1, -step, fast);
            return true;

        case Qt::Key_W:
            manual_move(2, step, fast);
            return true;
        case Qt::Key_S:
            manual_move(2, -step, fast);
            return true;

		case Qt::Key_R:
            manual_move(3, step, fast);
            return true;
        case Qt::Key_F:
            manual_move(3, -step, fast);
            return true;

		case Qt::Key_T:
            manual_move(4, step, fast);
            return true;
        case Qt::Key_G:
            manual_move(4, -step, fast);
            return true;
        }
    }
    return false;
}

