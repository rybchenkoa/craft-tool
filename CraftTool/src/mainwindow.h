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
    void load_file(char *fileName);

    bool eventFilter(QObject *object, QEvent *event);

public:
    Ui::MainWindow *ui;
    QTimer updateTimer;
    QTime time;

public slots:
    void menu_open_program();
    void update_state();
    void on_c_refHomeButton_clicked();
    void on_c_toZeroButton_clicked();
    void on_c_setX0Button_clicked();
    void on_c_setY0Button_clicked();
    void on_c_setZ0Button_clicked();
    void on_c_feedSlider_valueChanged(int value);
    void on_c_startButton_clicked();
	void on_c_runLineButton_clicked();
    void on_c_refreshTrajectory_clicked();
    void on_c_pauseButton_clicked();
    void on_c_breakButton_clicked();

	void on_c_topViewButton_clicked();
	void on_c_bottomViewButton_clicked();
	void on_c_frontViewButton_clicked();
	void on_c_backViewButton_clicked();
	void on_c_leftViewButton_clicked();
	void on_c_rightViewButton_clicked();
};

#endif // MAINWINDOW_H
