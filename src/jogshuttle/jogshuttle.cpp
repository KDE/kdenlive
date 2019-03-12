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

#include "kdenlive_debug.h"

#include <QApplication>
#include <QDir>
#include <QEvent>
#include <cerrno>
#include <cstring>
#include <sys/select.h>
#include <utility>
// according to earlier standards
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

// init media event type constants
const QEvent::Type MediaCtrlEvent::Key = (QEvent::Type)QEvent::registerEventType();
const QEvent::Type MediaCtrlEvent::Jog = (QEvent::Type)QEvent::registerEventType();
const QEvent::Type MediaCtrlEvent::Shuttle = (QEvent::Type)QEvent::registerEventType();

ShuttleThread::ShuttleThread(QString device, QObject *parent)
    : m_device(std::move(device))
    , m_parent(parent)
    , m_isRunning(false)
{
}

ShuttleThread::~ShuttleThread() = default;

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
    media_ctrl mc;
    media_ctrl_open_dev(&mc, m_device.toUtf8().data());
    if (mc.fd < 0) {
        return;
    }
    fd_set readset;
    struct timeval timeout;
    // enter thread loop
    m_isRunning = true;
    while (m_isRunning) {
        // reset the read set
        FD_ZERO(&readset);
        FD_SET(mc.fd, &readset);
        // reinit the timeout structure
        timeout.tv_sec = 0;
        timeout.tv_usec = 400000;
        // do select in blocked mode and wake up after timeout
        // for stop_me evaluation
        int result = select(mc.fd + 1, &readset, nullptr, nullptr, &timeout);
        // see if there was an error or timeout else process event
        if (result < 0 && errno == EINTR) {
            // EINTR event caught. This is not a problem - continue processing
            continue;
        }
        if (result < 0) {
            // stop thread
            m_isRunning = false;
        } else if (result > 0) {
            // we have input
            if (FD_ISSET(mc.fd, &readset)) {
                media_ctrl_event mev;
                mev.type = MEDIA_CTRL_EVENT_NONE;
                // read input
                media_ctrl_read_event(&mc, &mev);
                // process event
                handleEvent(mev);
            }
        } else if (result == 0) {
            // on timeout
        }
    }
    // close the handle and return thread
    media_ctrl_close(&mc);
}

void ShuttleThread::handleEvent(const media_ctrl_event &ev)
{
    if (ev.type == MEDIA_CTRL_EVENT_KEY) {
        key(ev);
    } else if (ev.type == MEDIA_CTRL_EVENT_JOG) {
        jog(ev);
    } else if (ev.type == MEDIA_CTRL_EVENT_SHUTTLE) {
        shuttle(ev);
    }
}

void ShuttleThread::key(const media_ctrl_event &ev)
{
    if (ev.value == KEY_PRESS) {
        QApplication::postEvent(m_parent, new MediaCtrlEvent(MediaCtrlEvent::Key, ev.index + 1));
    }
}

void ShuttleThread::shuttle(const media_ctrl_event &ev)
{
    int value = ev.value / 2;

    if (value > MaxShuttleRange || value < -MaxShuttleRange) {
        // qCDebug(KDENLIVE_LOG) << "Jog shuttle value is out of range: " << MaxShuttleRange;
        return;
    }
    QApplication::postEvent(m_parent, new MediaCtrlEvent(MediaCtrlEvent::Shuttle, value));
}

void ShuttleThread::jog(const media_ctrl_event &ev)
{
    QApplication::postEvent(m_parent, new MediaCtrlEvent(MediaCtrlEvent::Jog, ev.value));
}

JogShuttle::JogShuttle(const QString &device, QObject *parent)
    : QObject(parent)
    , m_shuttleProcess(device, this)
{
    m_shuttleProcess.start();
}

JogShuttle::~JogShuttle()
{
    stopDevice();
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
            qCWarning(KDENLIVE_LOG) << "Needed to force jogshuttle process termination";
        }
    }
}

void JogShuttle::customEvent(QEvent *e)
{
    QEvent::Type type = e->type();

    if (type == MediaCtrlEvent::Key) {
        auto *mev = static_cast<MediaCtrlEvent *>(e);
        emit button(mev->value());
    } else if (type == MediaCtrlEvent::Jog) {
        auto *mev = static_cast<MediaCtrlEvent *>(e);
        int value = mev->value();

        if (value < 0) {
            emit jogBack();
        } else if (value > 0) {
            emit jogForward();
        }
    } else if (type == MediaCtrlEvent::Shuttle) {
        auto *mev = static_cast<MediaCtrlEvent *>(e);
        emit shuttlePos(mev->value());
    }
}

QString JogShuttle::canonicalDevice(const QString &device)
{
    return QDir(device).canonicalPath();
}

DeviceMap JogShuttle::enumerateDevices(const QString &devPath)
{
    DeviceMap devs;
    QDir devDir(devPath);

    if (!devDir.exists()) {
        return devs;
    }

    const QStringList fileList = devDir.entryList(QDir::System | QDir::Files);
    for (const QString &fileName : fileList) {
        QString devFullPath = devDir.absoluteFilePath(fileName);
        QString fileLink = JogShuttle::canonicalDevice(devFullPath);
        // qCDebug(KDENLIVE_LOG) << QString(" [%1] ").arg(fileName);
        // qCDebug(KDENLIVE_LOG) << QString(" [%1] ").arg(fileLink);

        media_ctrl mc;
        media_ctrl_open_dev(&mc, (char *)fileLink.toUtf8().data());
        if (mc.fd > 0 && (mc.device != nullptr)) {
            devs.insert(QString(mc.device->name), devFullPath);
            qCDebug(KDENLIVE_LOG) << QStringLiteral(" [keys-count=%1] ").arg(media_ctrl_get_keys_count(&mc));
        }
        media_ctrl_close(&mc);
    }

    return devs;
}

int JogShuttle::keysCount(const QString &devPath)
{
    media_ctrl mc;
    int keysCount = 0;

    QString fileLink = canonicalDevice(devPath);
    media_ctrl_open_dev(&mc, (char *)fileLink.toUtf8().data());
    if (mc.fd > 0 && (mc.device != nullptr)) {
        keysCount = media_ctrl_get_keys_count(&mc);
    }

    return keysCount;
}
