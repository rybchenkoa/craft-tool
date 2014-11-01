#pragma once

#include <QPlainTextEdit>

class LogConsole : public QPlainTextEdit
{
   Q_OBJECT
public:
   explicit LogConsole(QWidget *parent = 0);
   void output(QString);
   void scrollDown();
protected:
   void keyPressEvent(QKeyEvent *){}
   void mousePressEvent(QMouseEvent *);
   void mouseDoubleClickEvent(QMouseEvent *){}
   void contextMenuEvent(QContextMenuEvent *){}
};

