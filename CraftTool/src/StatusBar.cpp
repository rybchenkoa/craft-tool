#include <iomanip>
#include <QTimerEvent>
#include "StatusBar.h"
#include "mainwindow.h"
#include "GCodeInterpreter.h"
#include "RemoteDevice.h"

StatusBar::StatusBar(QWidget *parent) : QStatusBar(parent)
{
    timer.start(200, this);
}

void StatusBar::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == timer.timerId())
    {
        RemoteDevice *device = (RemoteDevice*)g_inter->remoteDevice;
        if(device == 0)
            return;
        auto connection = device->connection;
        if(connection == 0)
            return;

        int elapsedSec = g_mainWindow->time.elapsed() / 1000;
        int elapsedMin = elapsedSec / 60;
        elapsedSec = elapsedSec % 60;
        int elapsedHour = elapsedMin / 60;
        elapsedMin = elapsedMin % 60;

        std::stringstream ss;
        ss << "send crc err: " << device->missedHalfSend
          << ", recv crc err: " << device->missedReceives
          << ", T: " << device->send_lag_ms()
          << ", missed: " << device->missedSends
          << ", pack err: " << connection->errs
          << ", read: " << connection->receiveBPS
          << ", write: " << connection->transmitBPS
          << ", spack: " << device->packSends
          << ", line " << device->get_current_line()
          << " time " << elapsedHour << ":" << std::setw(2) << std::setfill('0') << elapsedMin << ":" << std::setw(2) << elapsedSec;
        showMessage(ss.str().c_str());
    }
    else
    {
        QStatusBar::timerEvent(event);
    }
}
