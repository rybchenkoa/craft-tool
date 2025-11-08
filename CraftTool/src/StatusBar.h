#pragma once

#include <QStatusBar>
#include <QBasicTimer>

class StatusBar : public QStatusBar
{
    Q_OBJECT

public:
    StatusBar(QWidget *parent = 0);
    //~StatusBar();

protected:
    void timerEvent(QTimerEvent *event);

private:
    QBasicTimer timer;
};
