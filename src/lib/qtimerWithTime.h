/***************************************************************************
 *   Copyright (C) 2012    Simon A. Eugster <simon.eu@gmail.com>           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

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
