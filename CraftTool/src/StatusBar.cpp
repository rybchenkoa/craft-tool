#include <QTimerEvent>
#include "StatusBar.h"
#include "main.h"

StatusBar::StatusBar(QWidget *parent) : QStatusBar(parent)
{
    timer.start(200, this);
}

void StatusBar::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == timer.timerId())
    {
        char text[500];
        CRemoteDevice *device = (CRemoteDevice*)g_inter.remoteDevice;
        auto port = device->comPort;
        sprintf(text, "send err: %d, missed: %d, recv err: %d, pack err: %d, read: %u, write: %u",
                device->missedHalfSend,
                device->missedSends,
                device->missedReceives,
                port->errs,
                port->receiveBPS,
                port->transmitBPS);
        showMessage(text);
    }
    else
    {
        QStatusBar::timerEvent(event);
    }
}
