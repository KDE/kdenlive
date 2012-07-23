/***************************************************************************
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#ifndef ABSTRACTMONITOR_H
#define ABSTRACTMONITOR_H

#include "definitions.h"

#include <QObject>
#include <QVector>
#include <QWidget>
#include <QImage>
#include <QPainter>
#include <QFrame>
#include <QTimer>

#include <stdint.h>

class MonitorManager;
class VideoContainer;

class AbstractRender: public QObject
{
Q_OBJECT public:

    /** @brief Build an abstract MLT Renderer
     *  @param name A unique identifier for this renderer
     *  @param winid The parent widget identifier (required for SDL display). Set to 0 for OpenGL rendering
     *  @param profile The MLT profile used for the renderer (default one will be used if empty). */
    explicit AbstractRender(Kdenlive::MONITORID name, QWidget *parent = 0):QObject(parent), sendFrameForAnalysis(false), analyseAudio(false), m_name(name) {};

    /** @brief Destroy the MLT Renderer. */
    virtual ~AbstractRender() {};

    /** @brief This property is used to decide if the renderer should convert it's frames to QImage for use in other Kdenlive widgets. */
    bool sendFrameForAnalysis;
    
    /** @brief This property is used to decide if the renderer should send audio data for monitoring. */
    bool analyseAudio;
    
    const QString &name() const {return m_name;};

    /** @brief Someone needs us to send again a frame. */
    virtual void sendFrameUpdate() = 0;

private:
    QString m_name;
    
signals:
    /** @brief The renderer refreshed the current frame. */
    void frameUpdated(QImage);

    /** @brief This signal contains the audio of the current frame. */
    void audioSamplesSignal(QVector<int16_t>,int,int,int);
};



class VideoSurface : public QWidget
{
    Q_OBJECT
public:
    VideoSurface(QWidget *parent = 0);
    
signals:
    void refreshMonitor();

protected:
    virtual void paintEvent ( QPaintEvent * event );
};


class AbstractMonitor : public QWidget
{
    Q_OBJECT
public:
    AbstractMonitor(Kdenlive::MONITORID id, MonitorManager *manager, QWidget *parent = 0);
    Kdenlive::MONITORID id() {return m_id;};
    virtual ~AbstractMonitor();
    virtual AbstractRender *abstractRender() = 0;
    bool isActive() const;
    VideoContainer *videoBox;
    VideoSurface *videoSurface;
    void createVideoSurface();
    
    
public slots:
    virtual void stop() = 0;
    virtual void start() = 0;
    virtual void slotPlay() = 0;
    virtual void slotMouseSeek(int eventDelta, bool fast) = 0;
    bool slotActivateMonitor();
    virtual void slotSwitchFullScreen() = 0;

protected:
    Kdenlive::MONITORID m_id;
    MonitorManager *m_monitorManager;
};

class VideoContainer : public QFrame
{
    Q_OBJECT
public:
    explicit VideoContainer(AbstractMonitor *monitor, QWidget *parent = 0);
    void switchFullScreen();

protected:
    virtual void mouseDoubleClickEvent(QMouseEvent * event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    void keyPressEvent(QKeyEvent *event);
    virtual void wheelEvent(QWheelEvent * event);

private:
    Qt::WindowFlags m_baseFlags;
    AbstractMonitor *m_monitor;
};

#endif
