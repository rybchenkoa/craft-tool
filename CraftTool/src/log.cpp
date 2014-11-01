#include "main.h"
#include "ui_mainwindow.h"
#include "log.h"

#define MAX_MESSAGE_SIZE 16000

Logger g_logger;

void Logger::log_console_string(QColor color, const char *value)
{
    emit send_string(color, value);
}

void log_console(QColor color, const char *format, ...)
{
    char buffer[MAX_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    g_logger.log_console_string(color, buffer);
}
