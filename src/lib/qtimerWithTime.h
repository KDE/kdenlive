#ifndef QTIMERWITHTIME_H
#define QTIMERWITHTIME_H

#include <QTimer>
#include <QTime>

class QTimerWithTime : public QTimer
{
    Q_OBJECT
public:
    virtual void start(int msec);
    int elapsed() const;
 private:
    QTime m_time;
};
#endif
