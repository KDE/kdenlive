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

#include <QEvent>
#include <QMap>
#include <QObject>
#include <QThread>

#include <media_ctrl/mediactrl.h>

class MediaCtrlEvent : public QEvent
{
public:
    MediaCtrlEvent(QEvent::Type type, int value)
        : QEvent(type)
        , m_value(value)
    {
    }

    int value() { return m_value; }

    static const QEvent::Type Key;
    static const QEvent::Type Jog;
    static const QEvent::Type Shuttle;

private:
    int m_value;
};

class ShuttleThread : public QThread
{

public:
    ShuttleThread(QString device, QObject *parent);
    ~ShuttleThread() override;
    void run() override;
    QString device();
    void stop();

private:
    enum { MaxShuttleRange = 7 };

    void handleEvent(const media_ctrl_event &ev);
    void jog(const media_ctrl_event &ev);
    void shuttle(const media_ctrl_event &ev);
    void key(const media_ctrl_event &ev);

    QString m_device;
    QObject *m_parent;
    volatile bool m_isRunning;
};

typedef QMap<QString, QString> DeviceMap;
typedef QMap<QString, QString>::iterator DeviceMapIter;

class JogShuttle : public QObject
{
    Q_OBJECT
public:
    explicit JogShuttle(const QString &device, QObject *parent = nullptr);
    ~JogShuttle() override;
    void stopDevice();
    void initDevice(const QString &device);
    static QString canonicalDevice(const QString &device);
    static DeviceMap enumerateDevices(const QString &devPath);
    static int keysCount(const QString &devPath);

protected:
    void customEvent(QEvent *e) override;

private:
    ShuttleThread m_shuttleProcess;

signals:
    void jogBack();
    void jogForward();
    void shuttlePos(int);
    void button(int);
};

#endif
