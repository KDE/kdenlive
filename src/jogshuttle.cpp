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



#define DELAY 10

#define KEY 1
#define KEY1 256
#define KEY2 257
#define KEY3 258
#define KEY4 259
#define KEY5 260
#define KEY6 261
#define KEY7 262
#define KEY8 263
#define KEY9 264
#define KEY10 265
#define KEY11 266
#define KEY12 267
#define KEY13 268
#define KEY14 269
#define KEY15 270

#define JOGSHUTTLE 2
#define JOG 7
#define SHUTTLE 8

#define JOG_BACK1 10001
#define JOG_FWD1 10002
#define JOG_FWD 10003
#define JOG_FWD_SLOW 10004
#define JOG_FWD_FAST 10005
#define JOG_BACK 10006
#define JOG_BACK_SLOW 10007
#define JOG_BACK_FAST 10008
#define JOG_STOP 10009


#include <QApplication>
#include <QEvent>

#include <KDebug>
#include <kde_file.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>



#include "jogshuttle.h"


void ShuttleThread::init(QObject *parent, QString device) {
    m_parent = parent;
    m_device = device;
    stop_me = false;
    m_isWorking = false;
    shuttlevalue = 0xffff;
    jogvalue = 0xffff;
}

bool ShuttleThread::isWorking() {
    return m_isWorking;
}

void ShuttleThread::run() {
    kDebug() << "-------  STARTING SHUTTLE: " << m_device;

    const int fd = KDE_open((char *) m_device.toUtf8().data(), O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Can't open Jog Shuttle FILE DESCRIPTOR\n");
        return;;
    }
    EV ev;

    if (ioctl(fd, EVIOCGRAB, 1) < 0) {
        fprintf(stderr, "Can't get exclusive access on  Jog Shuttle FILE DESCRIPTOR\n");
        return;;
    }

    while (!stop_me) {
        read(fd, &ev, sizeof(ev));
        handle_event(ev);
    }
    close(fd);

}

void ShuttleThread::handle_event(EV ev) {
    switch (ev.type) {
    case KEY :
        key(ev.code, ev.value);
        break;
    case JOGSHUTTLE :
        jogshuttle(ev.code, ev.value);
        break;
    }
}

void ShuttleThread::key(unsigned short code, unsigned int value) {
    if (value == 0) {
        // Button release (ignored)
        return;
    }

    code -= KEY1 - 1;

    // Bound check!
    if (code > 16)
        return;

    kDebug() << "Button PRESSED: " << code;
    QApplication::postEvent(m_parent, new QEvent((QEvent::Type)(20000 + code)));

}

void ShuttleThread::shuttle(int value) {
    //gettimeofday( &last_shuttle, 0 );
    //need_synthetic_shuttle = value != 0;

    if (value == shuttlevalue)
        return;
    shuttlevalue = value;
    switch (value) {
    case - 7 :
    case - 6 :
    case - 5 :
    case - 4 :
    case - 3 :
        QApplication::postEvent(m_parent, new QEvent((QEvent::Type) JOG_BACK_FAST));
        break;  // Reverse fast
    case - 2 :
        QApplication::postEvent(m_parent, new QEvent((QEvent::Type) JOG_BACK));
        break;  // Reverse
    case - 1 :
        QApplication::postEvent(m_parent, new QEvent((QEvent::Type) JOG_BACK_SLOW));
        break;  // Reverse slow
    case  0 :
        QApplication::postEvent(m_parent, new QEvent((QEvent::Type) JOG_STOP));
        break;  // Stop!
    case  1 :
        QApplication::postEvent(m_parent, new QEvent((QEvent::Type) JOG_FWD_SLOW));
        break;  // Forward slow
    case  2 :
        QApplication::postEvent(m_parent, new QEvent((QEvent::Type) JOG_FWD));
        break;  // Normal play
    case  3 :
    case  4 :
    case  5 :
    case  6 :
    case  7 :
        QApplication::postEvent(m_parent, new QEvent((QEvent::Type) JOG_FWD_FAST));
        break; // Fast forward
    }

}

void ShuttleThread::jog(unsigned int value) {
    // We should generate a synthetic event for the shuttle going
    // to the home position if we have not seen one recently
    //check_synthetic();

    if (jogvalue != 0xffff) {
        if (value < jogvalue)
            QApplication::postEvent(m_parent, new QEvent((QEvent::Type) JOG_BACK1));
        else if (value > jogvalue)
            QApplication::postEvent(m_parent, new QEvent((QEvent::Type) JOG_FWD1));
    }
    jogvalue = value;
}


void ShuttleThread::jogshuttle(unsigned short code, unsigned int value) {
    switch (code) {
    case JOG :
        jog(value);
        break;
    case SHUTTLE :
        shuttle(value);
        break;
    }
}


JogShuttle::JogShuttle(QString device, QObject *parent): QObject(parent) {
    initDevice(device);
}

JogShuttle::~JogShuttle() {
    if (m_shuttleProcess.isRunning()) m_shuttleProcess.exit();
}

void JogShuttle::initDevice(QString device) {
    if (m_shuttleProcess.isRunning()) return;
    m_shuttleProcess.init(this, device);
    m_shuttleProcess.start(QThread::LowestPriority);
}

void JogShuttle::stopDevice() {
    if (m_shuttleProcess.isRunning()) m_shuttleProcess.stop_me = true;
}

void JogShuttle::customEvent(QEvent* e) {
    switch (e->type()) {
    case JOG_BACK1:
        emit rewind1();
        break;
    case JOG_FWD1:
        emit forward1();
        break;
    case JOG_BACK:
        emit rewind(-1);
        break;
    case JOG_FWD:
        emit forward(1);
        break;
    case JOG_BACK_SLOW:
        emit rewind(-0.5);
        break;
    case JOG_FWD_SLOW:
        emit forward(0.5);
        break;
    case JOG_BACK_FAST:
        emit rewind(-2);
        break;
    case JOG_FWD_FAST:
        emit forward(2);
        break;
    case JOG_STOP:
        emit stop();
        break;
    default:
        emit button(e->type() - 20000);
    }
}



// #include "jogshuttle.moc"

