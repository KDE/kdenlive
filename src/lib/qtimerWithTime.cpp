/***************************************************************************
 *   SPDX-FileCopyrightText: 2012 Simon A. Eugster <simon.eu@gmail.com>           *
 *                                                                         *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 ***************************************************************************/

#include "qtimerWithTime.h"

void QTimerWithTime::start(int msec)
{
    QTimer::start(msec);
    m_time.start();
}

qint64 QTimerWithTime::elapsed() const
{
    return m_time.elapsed();
}

bool QTimerWithTime::isValid() const
{
    return m_time.isValid();
}
