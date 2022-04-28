/*
    SPDX-FileCopyrightText: 2011 Simon Andreas Eugster <simon.eu@gmail.com>
    This file is part of kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "audioscopes/abstractaudioscopewidget.h"
#include "colorscopes/abstractgfxscopewidget.h"

#include <QList>

class QDockWidget;
class AbstractMonitor;
class QSignalMapper;

/** @class ScopeManager
    @brief Manages communication between Scopes and Renderer.
  
    The scope manager handles the data transfer between the active Renderer and
    all scopes that have been registered via ScopeManager::addScope(AbstractAudioScopeWidget, QDockWidget)
    or ScopeManager::addScope(AbstractGfxScopeWidget, QDockWidget). It checks whether the renderer really
    needs to send data (it does not, for example, if no scopes are visible).
  */
class ScopeManager : public QObject
{
    Q_OBJECT

    struct GfxScopeData
    {
        AbstractGfxScopeWidget *scope;
        bool singleFrameRequested;
        GfxScopeData()
        {
            scope = nullptr;
            singleFrameRequested = false;
        }
    };

    struct AudioScopeData
    {
        AbstractAudioScopeWidget *scope;
        bool singleFrameRequested;
        AudioScopeData()
        {
            scope = nullptr;
            singleFrameRequested = false;
        }
    };

public:
    explicit ScopeManager(QObject *parent = nullptr);

    /**
      Adds a scope and sets up signal/slot connections to ensure that the scope
      receives data when required.
      \see addScope(AbstractGfxScopeWidget, QDockWidget)
      */
    bool addScope(AbstractAudioScopeWidget *audioScope, QDockWidget *audioScopeWidget = nullptr);
    /**
      \see addScope(AbstractAudioScopeWidget, QDockWidget)
      */
    bool addScope(AbstractGfxScopeWidget *colorScope, QDockWidget *colorScopeWidget = nullptr);

private:
    QList<AudioScopeData> m_audioScopes;
    QList<GfxScopeData> m_colorScopes;

    AbstractMonitor *m_lastConnectedRenderer{nullptr};

    QSignalMapper *m_signalMapper;

    /**
      Checks whether there is any scope accepting audio data, or if all of them are hidden
      or if auto refresh is disabled.
      \see imagesAcceptedByScopes(): Same for image data
      */
    bool audioAcceptedByScopes() const;
    /**
      \see audioAcceptedByScopes()
      */
    bool imagesAcceptedByScopes() const;

    /**
      Creates all the scopes in audioscopes/ and colorscopes/.
      New scopes are not detected automatically but have to be added.
     */
    void createScopes();

    /**
      Creates a dock for @param scopeWidget with the title @param title and
      adds it to the manager.
      @param scopeWidget has to be of type AbstractAudioScopeWidget or AbstractGfxScopeWidget (@see addScope).
     */
    template <class T> void createScopeDock(T *scopeWidget, const QString &title, const QString &name);

public slots:
    void slotCheckActiveScopes();

private slots:
    /**
      Updates the signal/slot connection since the active renderer has changed.
      */
    void slotUpdateActiveRenderer();
    /**
      The scope source was deleted, clear it.
      */
    void slotClearColorScopes();
    /**
      \see checkActiveAudioScopes()
      \see checkActiveColourScopes()
      */

    /**
    Checks whether audio data is required, and notifies the renderer (enable or disable data sending).
    \see checkActiveAudioScopes() for image data
    */
    void checkActiveAudioScopes();
    /**
      Checks whether any scope accepts frames, and notifies the renderer.
      \see checkActiveAudioScopes() for audio data
      */
    void checkActiveColourScopes();

    void slotDistributeFrame(const QImage &image);
    void slotDistributeAudio(const audioShortVector &sampleData, int freq, int num_channels, int num_samples);
    /**
      Allows a scope to explicitly request a new frame, even if the scope's autoRefresh is disabled.
      */
    void slotRequestFrame(const QString &widgetName);
    void slotScopeReady();
};
