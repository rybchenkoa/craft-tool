#include <QColor>
#include <QObject>

class Logger : public QObject
{
    Q_OBJECT
public:
    void log_console_string(QColor color, const char *value);
signals:
    void send_string(QColor color, QString value);
};

void log_console(QColor color, const char *format, ...);

#define log_message(format, ...) log_console(Qt::black, format, ##__VA_ARGS__)

#define log_warning(format, ...) log_console(Qt::red, format, ##__VA_ARGS__)

extern Logger g_logger;

inline std::string to_string(int value)
{
	return std::to_string((long long) value);
}

inline std::string to_string(double value)
{
	return std::to_string((long double) value);
}