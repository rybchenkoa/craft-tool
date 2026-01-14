#pragma once

#include <QApplication>
#include "mainwindow.h"
#include "GCodeInterpreter.h"
#include "RemoteDevice.h"

extern QApplication *g_application;
extern MainWindow *g_mainWindow;
extern Interpreter::GCodeInterpreter *g_inter;
extern RemoteDevice *g_device;
extern std::string appDir;