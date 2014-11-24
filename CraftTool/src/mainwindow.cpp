#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>

#include "GCodeInterpreter.h"
extern Interpreter::GCodeInterpreter g_inter;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->menuOpenProgram, SIGNAL(triggered()), this, SLOT(menuOpenProgram()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::menuOpenProgram()
{
    QString str = QFileDialog::getOpenFileName(0, "Open Dialog", "", "*.*");
    g_inter.read_file(str.toLocal8Bit().data());
}
