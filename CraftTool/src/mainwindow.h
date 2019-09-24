#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QTime>

namespace Ui {
class MainWindow;
}

class QLineEdit;
class QAbstractButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    bool connect_to_device();
    void load_file(QString fileName);

    bool eventFilter(QObject *object, QEvent *event);

public:
    Ui::MainWindow *ui;
    QTimer updateTimer;
    QTime time;
	std::vector<QAbstractButton*> buttonSet0;
	std::vector<QLineEdit*> textCoord;
	std::vector<QAbstractButton*> buttonPlusCoord;
	std::vector<QAbstractButton*> buttonMinusCoord;

public slots:
    void menu_open_program();
    void update_state();
    void on_c_refHomeButton_clicked();
    void on_c_toZeroButton_clicked();
    void on_c_feedSlider_valueChanged(int value);
    void on_c_startButton_clicked();
	void on_c_runLineButton_clicked();
    void on_c_refreshTrajectory_clicked();
    void on_c_pauseButton_clicked();
    void on_c_breakButton_clicked();
};

#endif // MAINWINDOW_H
