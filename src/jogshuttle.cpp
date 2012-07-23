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


#include "jogshuttle.h"

#include <KDebug>
#include <kde_file.h>

#include <QApplication>
#include <QEvent>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>

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

// Constants for the returned events when reading them from the device
#define JOGSHUTTLE 2
#define JOG 7
#define SHUTTLE 8

// Constants for the signals sent.
#define JOG_BACK1 10001
#define JOG_FWD1 10002
#define KEY_EVENT_OFFSET 20000

// middle value for shuttle, will be +/-MAX_SHUTTLE_RANGE
#define JOG_STOP 10020
#define MAX_SHUTTLE_RANGE 7

void ShuttleThread::init(QObject *parent, QString device)
{
    m_parent = parent;
    m_device = device;
    stop_me = false;
    m_isWorking = false;
    shuttlevalue = 0xffff;
    shuttlechange = false;
    jogvalue = 0xffff;
}

bool ShuttleThread::isWorking()
{
    return m_isWorking;
}

void ShuttleThread::run()
{
    kDebug() << "-------  STARTING SHUTTLE: " << m_device;

    const int fd = KDE_open((char *) m_device.toUtf8().data(), O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Can't open Jog Shuttle FILE DESCRIPTOR\n");
        return;;
    }
    EV ev;

    if (ioctl(fd, EVIOCGRAB, 1) < 0) {
        fprintf(stderr, "Can't get exclusive access on  Jog Shuttle FILE DESCRIPTOR\n");
        close(fd);
        return;;
    }

    int num_warnings = 0;
    while (!stop_me) {
        if (read(fd, &ev, sizeof(ev)) < 0) {
            if (num_warnings % 10000 == 0)
                fprintf(stderr, "Failed to read event from Jog Shuttle FILE DESCRIPTOR (repeated %d times)\n", num_warnings + 1);
            num_warnings++;
        }
        handle_event(ev);
    }
    close(fd);

}

void ShuttleThread::handle_event(EV ev)
{
    switch (ev.type) {
    case KEY :
        key(ev.code, ev.value);
        break;
    case JOGSHUTTLE :
        if (ev.code == JOG)
            jog(ev.value);
        if (ev.code == SHUTTLE)
            shuttle(ev.value);
        break;
    }
}

void ShuttleThread::key(unsigned short code, unsigned int value)
{
    if (value == 0) {
        // Button release (ignored)
        return;
    }

    // Check key index
    code -= KEY1 - 1;
    if (code > 16)
        return;

    kDebug() << "Button PRESSED: " << code;
    QApplication::postEvent(m_parent, new QEvent((QEvent::Type)(KEY_EVENT_OFFSET + code)));

}

void ShuttleThread::shuttle(int value)
{
    //gettimeofday( &last_shuttle, 0 );
    //need_synthetic_shuttle = value != 0;

    if (value == shuttlevalue)
        return;

    if (value > MAX_SHUTTLE_RANGE || value < -MAX_SHUTTLE_RANGE) {
        fprintf(stderr, "Jog Shuttle returned value of %d (should be between -%d ad +%d)", value, MAX_SHUTTLE_RANGE, MAX_SHUTTLE_RANGE);
        return;
    }
    shuttlevalue = value;
    shuttlechange = true;
    QApplication::postEvent(m_parent, new QEvent((QEvent::Type) (JOG_STOP + value)));
}

void ShuttleThread::jog(unsigned int value)
{
    // generate a synthetic event for the shuttle going
    // to the home position if we have not seen one recently.
    //if (shuttlevalue != 0) {
    //  QApplication::postEvent(m_parent, new QEvent((QEvent::Type) JOG_STOP));
    //  shuttlevalue = 0;
    //}

    // This code takes care of wrapping around the limits of the jog dial number (it is represented as a single byte, hence
    // wraps at the 0/255 boundary). I used 25 as the difference to make sure that even in heavy load, with the jog dial
    // turning fast and we miss some events that we do not mistakenly reverse the direction.
    // Note also that at least the Contour ShuttlePRO v2 does not send an event for the value 0, so at the wrap it will
    // need 2 events to go forward. But that is nothing we can do about...
    if (jogvalue != 0xffff) {
        //fprintf(stderr, "value=%d jogvalue=%d\n", value, jogvalue);
        bool wrap = abs(value - jogvalue) > 25;
        bool rewind = value < jogvalue;
        bool forward = value > jogvalue;
        if ((rewind && !wrap) || (forward && wrap))
            QApplication::postEvent(m_parent, new QEvent((QEvent::Type) JOG_BACK1));
        else if ((forward && !wrap) || (rewind && wrap))
            QApplication::postEvent(m_parent, new QEvent((QEvent::Type) JOG_FWD1));
        else if (!forward && !rewind && !shuttlechange)
            // An event without changing the jog value is sent after each shuttle change.
            // As the shuttle rest position does not get a shuttle event, only a non-position-changing jog event.
            // Hence we stop on this when we see 2 non-position-changing jog events in a row.
            QApplication::postEvent(m_parent, new QEvent((QEvent::Type) JOG_STOP));
    }
    jogvalue = value;
    shuttlechange = false;
}


JogShuttle::JogShuttle(QString device, QObject *parent) :
        QObject(parent)
{
    initDevice(device);
}

JogShuttle::~JogShuttle()
{
    if (m_shuttleProcess.isRunning()) m_shuttleProcess.exit();
}

void JogShuttle::initDevice(QString device)
{
    if (m_shuttleProcess.isRunning()) {
        if (device == m_shuttleProcess.m_device) return;
        stopDevice();
    }
    m_shuttleProcess.init(this, device);
    m_shuttleProcess.start(QThread::LowestPriority);
}

void JogShuttle::stopDevice()
{
    if (m_shuttleProcess.isRunning())
        m_shuttleProcess.stop_me = true;
}

void JogShuttle::customEvent(QEvent* e)
{
    int code = e->type();

    // Handle simple job events
    switch (code) {
    case JOG_BACK1:
        emit jogBack();
        return;
    case JOG_FWD1:
        emit jogForward();
        return;
    }

    int shuttle_pos = code - JOG_STOP;
    if (shuttle_pos >= -MAX_SHUTTLE_RANGE && shuttle_pos <= MAX_SHUTTLE_RANGE) {
        emit shuttlePos(shuttle_pos);
        return;
    }

    // we've got a key event.
    //fprintf(stderr, "Firing button event for #%d\n", e->type() - KEY_EVENT_OFFSET); // DBG
    emit button(e->type() - KEY_EVENT_OFFSET);
}



// #include "jogshuttle.moc"

