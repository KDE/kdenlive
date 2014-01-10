#include "qtimerWithTime.h"

void QTimerWithTime::start(int msec)
{
    QTimer::start(msec);
    m_time.start();
}

int QTimerWithTime::elapsed() const
{
    return m_time.elapsed();
}

#include "qtimerWithTime.moc"
