/*
    SPDX-FileCopyrightText: 2012 Simon A. Eugster <simon.eu@gmail.com>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef QTIMERWITHTIME_H
#define QTIMERWITHTIME_H

#include <QElapsedTimer>
#include <QTimer>

class QTimerWithTime : public QTimer
{
    Q_OBJECT
public:
    virtual void start(int msec);
    qint64 elapsed() const;
    bool isValid() const;

private:
    QElapsedTimer m_time;
};
#endif
