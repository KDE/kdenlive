/***************************************************************************
 *   Copyright (C) 2011 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "scopemanager.h"

#include <QDockWidget>

#include "definitions.h"
#include "monitormanager.h"
#include "kdenlivesettings.h"

//#define DEBUG_SM
#ifdef DEBUG_SM
#include <QtCore/QDebug>
#endif

ScopeManager::ScopeManager(MonitorManager *monitorManager) :
    m_monitorManager(monitorManager),
    m_lastConnectedRenderer(NULL)
{
    m_signalMapper = new QSignalMapper(this);

    bool b = true;
    b &= connect(m_monitorManager, SIGNAL(checkColorScopes()), this, SLOT(slotUpdateActiveRenderer()));
    b &= connect(m_monitorManager, SIGNAL(clearScopes()), this, SLOT(slotClearColorScopes()));
    b &= connect(m_signalMapper, SIGNAL(mapped(QString)), this, SLOT(slotRequestFrame(QString)));
    Q_ASSERT(b);

    slotUpdateActiveRenderer();
}

bool ScopeManager::addScope(AbstractAudioScopeWidget *audioScope, QDockWidget *audioScopeWidget)
{
    bool added = false;
    int exists = false;
    // Only add the scope if it does not exist yet in the list
    for (int i = 0; i < m_audioScopes.size(); i++) {
        if (m_audioScopes[i].scope == audioScope) {
            exists = true;
            break;
        }
    }
    if (!exists) {
        // Add scope to the list, set up signal/slot connections
#ifdef DEBUG_SM
        qDebug() << "Adding scope to scope manager: " << audioScope->widgetName();
#endif

        m_signalMapper->setMapping(audioScopeWidget, QString(audioScope->widgetName()));

        AudioScopeData asd;
        asd.scope = audioScope;
        asd.scopeDockWidget = audioScopeWidget;
        m_audioScopes.append(asd);

        bool b = true;
        b &= connect(audioScope, SIGNAL(requestAutoRefresh(bool)), this, SLOT(slotCheckActiveScopes()));
//         b &= connect(audioScope, SIGNAL(signalFrameRequest(QString)), this, SLOT(slotRequestFrame(QString)));
        if (audioScopeWidget != NULL) {
            b &= connect(audioScopeWidget, SIGNAL(visibilityChanged(bool)), this, SLOT(slotCheckActiveScopes()));
            b &= connect(audioScopeWidget, SIGNAL(visibilityChanged(bool)), m_signalMapper, SLOT(map()));
        }
        Q_ASSERT(b);

        added = true;
    }
    return added;
}
bool ScopeManager::addScope(AbstractGfxScopeWidget *colorScope, QDockWidget *colorScopeWidget)
{
    bool added = false;
    int exists = false;
    for (int i = 0; i < m_colorScopes.size(); i++) {
        if (m_colorScopes[i].scope == colorScope) {
            exists = true;
            break;
        }
    }
    if (!exists) {
#ifdef DEBUG_SM
        qDebug() << "Adding scope to scope manager: " << colorScope->widgetName();
#endif

        m_signalMapper->setMapping(colorScopeWidget, QString(colorScope->widgetName()));

        GfxScopeData gsd;
        gsd.scope = colorScope;
        gsd.scopeDockWidget = colorScopeWidget;
        m_colorScopes.append(gsd);

        bool b = true;
        b &= connect(colorScope, SIGNAL(requestAutoRefresh(bool)), this, SLOT(slotCheckActiveScopes()));
        b &= connect(colorScope, SIGNAL(signalFrameRequest(QString)), this, SLOT(slotRequestFrame(QString)));
        if (colorScopeWidget != NULL) {
            b &= connect(colorScopeWidget, SIGNAL(visibilityChanged(bool)), this, SLOT(slotCheckActiveScopes()));
            b &= connect(colorScopeWidget, SIGNAL(visibilityChanged(bool)), m_signalMapper, SLOT(map()));
        }
        Q_ASSERT(b);

        added = true;
    }
    return added;
}


void ScopeManager::slotDistributeAudio(QVector<int16_t> sampleData, int freq, int num_channels, int num_samples)
{
#ifdef DEBUG_SM
    qDebug() << "ScopeManager: Starting to distribute audio.";
#endif
    for (int i = 0; i < m_audioScopes.size(); i++) {
        // Distribute audio to all scopes that are visible and want to be refreshed
        if (!m_audioScopes[i].scope->visibleRegion().isEmpty()) {
            if (m_audioScopes[i].scope->autoRefreshEnabled()) {
                m_audioScopes[i].scope->slotReceiveAudio(sampleData, freq, num_channels, num_samples);
#ifdef DEBUG_SM
                qDebug() << "ScopeManager: Distributed audio to " << m_audioScopes[i].scope->widgetName();
#endif
            }
        }
    }

    checkActiveAudioScopes();
}
void ScopeManager::slotDistributeFrame(const QImage image)
{
#ifdef DEBUG_SM
    qDebug() << "ScopeManager: Starting to distribute frame.";
#endif
    for (int i = 0; i < m_colorScopes.size(); i++) {
        if (!m_colorScopes[i].scope->visibleRegion().isEmpty()) {
            if (m_colorScopes[i].scope->autoRefreshEnabled()) {
                m_colorScopes[i].scope->slotRenderZoneUpdated(image);
#ifdef DEBUG_SM
                qDebug() << "ScopeManager: Distributed frame to " << m_colorScopes[i].scope->widgetName();
#endif
            } else if (m_colorScopes[i].singleFrameRequested) {
                // Special case: Auto refresh is disabled, but user requested an update (e.g. by clicking).
                // Force the scope to update.
                m_colorScopes[i].singleFrameRequested = false;
                m_colorScopes[i].scope->slotRenderZoneUpdated(image);
                m_colorScopes[i].scope->forceUpdateScope();
#ifdef DEBUG_SM
                qDebug() << "ScopeManager: Distributed forced frame to " << m_colorScopes[i].scope->widgetName();
#endif
            }
        }
    }

    checkActiveColourScopes();
}


void ScopeManager::slotRequestFrame(const QString widgetName)
{
#ifdef DEBUG_SM
    qDebug() << "ScopeManager: New frame was requested by " << widgetName;
#endif

    // Search for the scope in the lists and tag it to trigger a forced update
    // in the distribution slots
    for (int i = 0; i < m_colorScopes.size(); i++) {
        if (m_colorScopes[i].scope->widgetName() == widgetName) {
            m_colorScopes[i].singleFrameRequested = true;
            break;
        }
    }
    for (int i = 0; i < m_audioScopes.size(); i++) {
        if (m_audioScopes[i].scope->widgetName() == widgetName) {
            m_audioScopes[i].singleFrameRequested = true;
            break;
        }
    }
    if (m_lastConnectedRenderer) m_lastConnectedRenderer->sendFrameUpdate();
}


void ScopeManager::slotClearColorScopes()
{
    m_lastConnectedRenderer = NULL;
}


void ScopeManager::slotUpdateActiveRenderer()
{
    bool b = true;

    // Disconnect old connections
    if (m_lastConnectedRenderer != NULL) {
#ifdef DEBUG_SM
        qDebug() << "Disconnected previous renderer: " << m_lastConnectedRenderer->name();
#endif
        b &= m_lastConnectedRenderer->disconnect(this);
        Q_ASSERT(b);
    }

    m_lastConnectedRenderer = m_monitorManager->activeRenderer();

    // Connect new renderer
    if (m_lastConnectedRenderer != NULL) {
        b &= connect(m_lastConnectedRenderer, SIGNAL(frameUpdated(const QImage)),
                this, SLOT(slotDistributeFrame(const QImage)), Qt::UniqueConnection);
        b &= connect(m_lastConnectedRenderer, SIGNAL(audioSamplesSignal(QVector<int16_t>,int,int,int)),
                this, SLOT(slotDistributeAudio(QVector<int16_t>,int,int,int)), Qt::UniqueConnection);
        Q_ASSERT(b);

#ifdef DEBUG_SM
        qDebug() << "Renderer connected to ScopeManager: " << m_lastConnectedRenderer->name();
#endif

        if (imagesAcceptedByScopes()) {
#ifdef DEBUG_SM
            qDebug() << "Some scopes accept images, triggering frame update.";
#endif
            m_lastConnectedRenderer->sendFrameUpdate();
        }
    }
}


void ScopeManager::slotCheckActiveScopes()
{
#ifdef DEBUG_SM
    qDebug() << "Checking active scopes ...";
#endif
    checkActiveAudioScopes();
    checkActiveColourScopes();
}


bool ScopeManager::audioAcceptedByScopes() const
{
    bool accepted = false;
    for (int i = 0; i < m_audioScopes.size(); i++) {
        if (!m_audioScopes[i].scope->visibleRegion().isEmpty() && m_audioScopes[i].scope->autoRefreshEnabled()) {
            accepted = true;
            break;
        }
    }
#ifdef DEBUG_SM
    qDebug() << "Any scope accepting audio? " << accepted;
#endif
    return accepted;
}
bool ScopeManager::imagesAcceptedByScopes() const
{
    bool accepted = false;
    for (int i = 0; i < m_colorScopes.size(); i++) {
        if (!m_colorScopes[i].scope->visibleRegion().isEmpty() && m_colorScopes[i].scope->autoRefreshEnabled()) {
            accepted = true;
            break;
        }
    }
#ifdef DEBUG_SM
    qDebug() << "Any scope accepting images? " << accepted;
#endif
    return accepted;
}



void ScopeManager::checkActiveAudioScopes()
{
    bool audioStillRequested = audioAcceptedByScopes();

#ifdef DEBUG_SM
    qDebug() << "ScopeManager: New audio data still requested? " << audioStillRequested;
#endif

    KdenliveSettings::setMonitor_audio(audioStillRequested);
    m_monitorManager->slotUpdateAudioMonitoring();
}
void ScopeManager::checkActiveColourScopes()
{
    bool imageStillRequested = imagesAcceptedByScopes();

#ifdef DEBUG_SM
    qDebug() << "ScopeManager: New frames still requested? " << imageStillRequested;
#endif

    // Notify monitors whether frames are still required
    Monitor *monitor;
    monitor = static_cast<Monitor*>( m_monitorManager->monitor(Kdenlive::projectMonitor) );
    if (monitor != NULL) { 
	if (monitor->effectSceneDisplayed()) monitor->render->sendFrameForAnalysis = true;
	else monitor->render->sendFrameForAnalysis = imageStillRequested;
    }

    monitor = static_cast<Monitor*>( m_monitorManager->monitor(Kdenlive::clipMonitor) );
    if (monitor != NULL) { monitor->render->sendFrameForAnalysis = imageStillRequested; }

    RecMonitor *recMonitor = static_cast<RecMonitor*>( m_monitorManager->monitor(Kdenlive::recordMonitor) );
    if (recMonitor != NULL) { recMonitor->analyseFrames(imageStillRequested); }
}

