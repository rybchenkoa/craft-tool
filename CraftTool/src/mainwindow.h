#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QTime>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    bool connect_to_device();
    void run_file(char *fileName);

public:
    Ui::MainWindow *ui;
    QTimer updateTimer;
    QTime time;

public slots:
    void menu_open_program();
    void update_state();
};

#endif // MAINWINDOW_H
