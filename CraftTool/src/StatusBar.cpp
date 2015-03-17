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
        if(device == 0)
            return;
        auto port = device->comPort;
        if(port == 0)
            return;
        sprintf(text, "send err: %d, missed: %d, recv err: %d, pack err: %d, read: %u, write: %u",
                device->missedHalfSend,
                device->missedSends,
                device->missedReceives,
                port->errs,
                port->receiveBPS,
                port->transmitBPS);
        sprintf(text + strlen(text), " x, y, z {%f, %f, %f}",
                float(device->currentCoords[0]),
                float(device->currentCoords[1]),
                float(device->currentCoords[2]));
        sprintf(text + strlen(text), " line %d",
                device->get_current_line());
        showMessage(text);
    }
    else
    {
        QStatusBar::timerEvent(event);
    }
}
