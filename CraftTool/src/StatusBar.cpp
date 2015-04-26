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
        CRemoteDevice *device = (CRemoteDevice*)g_inter->remoteDevice;
        if(device == 0)
            return;
        auto port = device->comPort;
        if(port == 0)
            return;

        int elapsedSec = g_mainWindow->time.elapsed() / 1000;
        int elapsedMin = elapsedSec / 60;
        elapsedSec = elapsedSec % 60;
        int elapsedHour = elapsedMin / 60;
        elapsedMin = elapsedMin % 60;

        sprintf(text, "send err: %d, missed: %d, recv err: %d, pack err: %d, read: %u, write: %u",
                device->missedHalfSend,
                device->missedSends,
                device->missedReceives,
                port->errs,
                port->receiveBPS,
                port->transmitBPS);
        sprintf(text + strlen(text), " x, y, z {%f, %f, %f}",
                float(device->currentCoords.r[0]),
                float(device->currentCoords.r[1]),
                float(device->currentCoords.r[2]));
        sprintf(text + strlen(text), " line %d",
                device->get_current_line());
        sprintf(text + strlen(text), " time %d:%02d:%02d",
                elapsedHour,
                elapsedMin,
                elapsedSec);
        showMessage(text);
    }
    else
    {
        QStatusBar::timerEvent(event);
    }
}
