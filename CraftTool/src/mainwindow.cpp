#include <QFileDialog>
#include "mainwindow.h"
#include "ui_mainwindow.h"


#include "GCodeInterpreter.h"
extern Interpreter::GCodeInterpreter g_inter;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->menuOpenProgram, SIGNAL(triggered()), this, SLOT(menu_open_program()));
    connect(&updateTimer, SIGNAL(timeout()), this, SLOT(update_state()));

    updateTimer.start(100); //10 fps
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::menu_open_program()
{
    QString str = QFileDialog::getOpenFileName(0, "Open Dialog", "", "*.*");
    g_inter.read_file(str.toLocal8Bit().data());
}

void MainWindow::update_state()
{
    int currentLine = g_inter.remoteDevice->get_current_line();
    ui->c_commandList->setCurrentRow(currentLine);
}
