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

#include <cstdint>

#include <QImage>
#include <QObject>
#include <QWidget>

class MonitorManager;

class AbstractRender : public QObject
{
    Q_OBJECT public :

        /** @brief Build an abstract MLT Renderer
         *  @param name A unique identifier for this renderer
         *  @param winid The parent widget identifier (required for SDL display). Set to 0 for OpenGL rendering
         *  @param profile The MLT profile used for the renderer (default one will be used if empty). */
        explicit AbstractRender(Kdenlive::MonitorId name, QWidget *parent = nullptr)
        : QObject(parent)
        , sendFrameForAnalysis(false)
        , analyseAudio(false)
        , m_id(name)
    {
    }

    /** @brief Destroy the MLT Renderer. */
    ~AbstractRender() override = default;

    /** @brief This property is used to decide if the renderer should convert it's frames to QImage for use in other Kdenlive widgets. */
    bool sendFrameForAnalysis;

    /** @brief This property is used to decide if the renderer should send audio data for monitoring. */
    bool analyseAudio;

    Kdenlive::MonitorId id() const { return m_id; }

    /** @brief Someone needs us to send again a frame. */
    virtual void sendFrameUpdate() = 0;

private:
    Kdenlive::MonitorId m_id;

signals:
    /** @brief The renderer refreshed the current frame. */
    void frameUpdated(const QImage &);

    /** @brief This signal contains the audio of the current frame. */
    void audioSamplesSignal(const audioShortVector &, int, int, int);
    /** @brief Scopes are ready to receive a new frame. */
    void scopesClear();
};

class AbstractMonitor : public QWidget
{
    Q_OBJECT
public:
    AbstractMonitor(Kdenlive::MonitorId id, MonitorManager *manager, QWidget *parent = nullptr);
    Kdenlive::MonitorId id() { return m_id; }
    ~AbstractMonitor() override;
    bool isActive() const;
    virtual void mute(bool mute, bool updateIconOnly = false) = 0;

public slots:
    virtual void stop() = 0;
    virtual void start() = 0;
    virtual void slotPlay() = 0;
    virtual void slotRewind(double speed = 0) = 0;
    virtual void slotForward(double speed = 0, bool allowNormalPlay = false) = 0;
    virtual void refreshMonitorIfActive(bool directUpdate = false) = 0;
    virtual void slotMouseSeek(int eventDelta, uint modifiers) = 0;
    bool slotActivateMonitor();
    virtual void slotSwitchFullScreen(bool minimizeOnly = false) = 0;

protected:
    Kdenlive::MonitorId m_id;
    MonitorManager *m_monitorManager;

signals:
    /** @brief Send a frame for analysis or title background display. */
    void frameUpdated(const QImage &);
    /** @brief This signal contains the audio of the current frame. */
    void audioSamplesSignal(const audioShortVector &, int, int, int);
    /** @brief Scopes are ready to receive a new frame. */
    void scopesClear();
};

#endif
