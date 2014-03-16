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
#include <QDir>

#include <string.h>
#include <errno.h>
#include <sys/select.h>
// according to earlier standards
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>


// Constants for the signals sent.
#define JOG_BACK1 10001
#define JOG_FWD1 10002
#define KEY_EVENT_OFFSET 20000

// middle value for shuttle, will be +/-MAX_SHUTTLE_RANGE
#define JOG_STOP 10020
#define MAX_SHUTTLE_RANGE 7

void ShuttleThread::init(QObject *parent, const QString &device)
{
    m_parent = parent;
    m_device = device;
    m_isRunning = true;
}

QString ShuttleThread::device()
{
    return m_device;
}

void ShuttleThread::stop()
{
    m_isRunning = false;
}

void ShuttleThread::run()
{
	kDebug() << "-------  STARTING SHUTTLE: " << m_device;
	struct media_ctrl mc;

    // open device
    media_ctrl_open_dev(&mc, m_device.toUtf8().data());

    // on failure return
    if (mc.fd < 0) {
        kDebug() << "Can't open Jog Shuttle FILE DESCRIPTOR";
        return;
    }

    // init
    int result;
	fd_set readset;
	struct timeval timeout;

	// enter thread loop
	while (m_isRunning) {
		// reset the read set
		FD_ZERO(&readset);
		FD_SET(mc.fd, &readset);

		// reinit the timeout structure
		timeout.tv_sec  = 0;
		timeout.tv_usec = 400000; /* 400 ms */

		// do select in blocked mode and wake up after timeout
		// for stop_me evaluation
		result = select(mc.fd + 1, &readset, NULL, NULL, &timeout);

		// see if there was an error or timeout else process event
		if (result < 0 && errno == EINTR) {
			// EINTR event catched. This is not a problem - continue processing
			kDebug() << strerror(errno) << "\n";
			// continue processing
			continue;
		} else if (result < 0) {
			// stop thread
		    m_isRunning = false;
			kDebug() << strerror(errno) << "\n";
		} else if (result > 0) {
			// we have input
			if (FD_ISSET(mc.fd, &readset)) {
			    struct media_ctrl_event mev;
			    mev.type = MEDIA_CTRL_EVENT_NONE;
                // read input
				media_ctrl_read_event(&mc, &mev);
                // process event
                handleEvent(mev);
			}
		} else if (result == 0) {
		    // on timeout. let it here for debug
		}
	}

    kDebug() << "-------  STOPPING SHUTTLE: ";
	// close the handle and return thread
    media_ctrl_close(&mc);
}

void ShuttleThread::handleEvent(const struct media_ctrl_event& ev)
{
    if (ev.type == MEDIA_CTRL_EVENT_KEY)
        key(ev);
    else if (ev.type == MEDIA_CTRL_EVENT_JOG)
        jog(ev);
    else if (ev.type == MEDIA_CTRL_EVENT_SHUTTLE)
        shuttle(ev);
}

void ShuttleThread::key(const struct media_ctrl_event& ev)
{
    if (ev.value == KEY_PRESS) {
        int code = ev.index + 1;
        QApplication::postEvent(m_parent,
            new QEvent((QEvent::Type)(KEY_EVENT_OFFSET + code)));
    }
}

void ShuttleThread::shuttle(const struct media_ctrl_event& ev)
{
    int value = ev.value / 2;

    if (value > MAX_SHUTTLE_RANGE || value < -MAX_SHUTTLE_RANGE) {
        kDebug() << "Jog shuttle value is out of range: " << MAX_SHUTTLE_RANGE;
        return;
    }

    QApplication::postEvent(m_parent,
        new QEvent((QEvent::Type) (JOG_STOP + (value))));
}

void ShuttleThread::jog(const struct media_ctrl_event& ev)
{
    if (ev.value < 0)
        QApplication::postEvent(m_parent, new QEvent((QEvent::Type) JOG_BACK1));
    else if (ev.value > 0)
        QApplication::postEvent(m_parent, new QEvent((QEvent::Type) JOG_FWD1));
}

JogShuttle::JogShuttle(const QString &device, QObject *parent) :
        QObject(parent)
{
    initDevice(device);
}

JogShuttle::~JogShuttle()
{
	stopDevice();
}

void JogShuttle::initDevice(const QString &device)
{
    if (m_shuttleProcess.isRunning()) {
        if (device == m_shuttleProcess.device())
            return;

        stopDevice();
    }

    m_shuttleProcess.init(this, device);
    m_shuttleProcess.start(QThread::LowestPriority);
}

void JogShuttle::stopDevice()
{
    if (m_shuttleProcess.isRunning()) {
    	// tell thread to stop
        m_shuttleProcess.stop();
        m_shuttleProcess.quit();
        // give the thread some time (ms) to shutdown
        m_shuttleProcess.wait(600);

        // if still running - do it in the hardcore way
        if (m_shuttleProcess.isRunning()) {
        	m_shuttleProcess.terminate();
            kDebug() << "/// terminate jogshuttle process";
        }
    }
}

void JogShuttle::customEvent(QEvent* e)
{
    int code = e->type();

    // handle simple job events
    switch (code) {
        case JOG_BACK1:
            emit jogBack();
            return;
        case JOG_FWD1:
            emit jogForward();
            return;
    }

    // FIXME: this is a bad shuttle indication
    int shuttle_pos = code - JOG_STOP;
    if (shuttle_pos >= -MAX_SHUTTLE_RANGE && shuttle_pos <= MAX_SHUTTLE_RANGE) {
        emit shuttlePos(shuttle_pos);
        return;
    }

    // we've got a key event.
    emit button(e->type() - KEY_EVENT_OFFSET);
}

QString JogShuttle::canonicalDevice(const QString& device)
{
    return QDir(device).canonicalPath();
}

DeviceMap JogShuttle::enumerateDevices(const QString& devPath)
{
    DeviceMap devs;
    QDir devDir(devPath);

    if (!devDir.exists()) {
        return devs;
    }

    QStringList fileList = devDir.entryList(QDir::System | QDir::Files);
    foreach (const QString &fileName, fileList) {
        QString devFullPath = devDir.absoluteFilePath(fileName);
        QString fileLink = JogShuttle::canonicalDevice(devFullPath);
        kDebug() << QString(" [%1] ").arg(fileName);
        kDebug() << QString(" [%1] ").arg(fileLink);

        struct media_ctrl mc;
        media_ctrl_open_dev(&mc, (char*)fileLink.toUtf8().data());
        if (mc.fd > 0 && mc.device) {
            devs.insert(QString(mc.device->name), devFullPath);
            kDebug() <<  QString(" [keys-count=%1] ").arg(
                    media_ctrl_get_keys_count(&mc));
        }
        media_ctrl_close(&mc);
    }

    return devs;
}

int JogShuttle::keysCount(const QString& devPath)
{
    struct media_ctrl mc;
    int keysCount = 0;

    QString fileLink = canonicalDevice(devPath);
    media_ctrl_open_dev(&mc, (char*)fileLink.toUtf8().data());
    if (mc.fd > 0 && mc.device) {
        keysCount = media_ctrl_get_keys_count(&mc);
    }

    return keysCount;
}


#include "jogshuttle.moc"
