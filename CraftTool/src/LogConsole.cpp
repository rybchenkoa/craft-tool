#include "LogConsole.h"
#include <QScrollBar>

LogConsole::LogConsole(QWidget *parent) : QPlainTextEdit(parent)
{
}

void LogConsole::mousePressEvent(QMouseEvent *)
{
    setFocus();
}

void LogConsole::output(QColor color, QString message)
{
    bool needScroll;
    QScrollBar *vbar = verticalScrollBar();
    needScroll = vbar->value() == vbar->maximum();

    textCursor().insertBlock();
    QTextCharFormat format;
    format.setForeground(color);
    textCursor().setBlockCharFormat(format);
    textCursor().insertText(message);

    if(needScroll)
        scrollDown();
}

void LogConsole::scrollDown()
{
    QScrollBar *vbar = verticalScrollBar();
    vbar->setValue(vbar->maximum());
}
