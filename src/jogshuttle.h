/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   Based on code by Arendt David <admin@prnet.org>                       *
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

#ifndef SHUTTLE_H
#define SHUTTLE_H

#include <qthread.h>
#include <QObject>

#include <linux/input.h>


typedef struct input_event EV;

class ShuttleThread : public QThread {

public:
    virtual void run();
    void init(QObject *parent, QString device);
    QObject *m_parent;
    int shuttlevalue;
    unsigned short jogvalue;
    bool isWorking();
    bool stop_me;

private:
    QString m_device;
    bool m_isWorking;
    void handle_event(EV ev);
    void jogshuttle(unsigned short code, unsigned int value);
    void jog(unsigned int value);
    void shuttle(int value);
    void key(unsigned short code, unsigned int value);
};


class JogShuttle: public QObject {
Q_OBJECT public:
    JogShuttle(QString device, QObject * parent = 0);
    ~JogShuttle();
    void stopDevice();
    void initDevice(QString device);

protected:
    virtual void customEvent(QEvent * e);

private:
    ShuttleThread m_shuttleProcess;

signals:
    void rewind1();
    void forward1();
    void rewind(double);
    void forward(double);
    void stop();
    void button(int);
};

#endif
