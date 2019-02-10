/***************************************************************************
                          effecstackedit.cpp  -  description
                             -------------------
    begin                : Mar 15 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "transitionsettings.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "project/projectmanager.h"

#include "klocalizedstring.h"

#include <QMimeData>

TransitionSettings::TransitionSettings(Monitor *monitor, QWidget *parent)
    : QWidget(parent)
    , m_usedTransition(nullptr)
    , m_autoTrackTransition(0)
{
    setupUi(this);
    auto *vbox1 = new QVBoxLayout(frame);
    m_effectEdit = new EffectStackEdit(monitor, frame);
    vbox1->setContentsMargins(0, 0, 0, 0);
    vbox1->setSpacing(0);
    vbox1->addWidget(m_effectEdit);
    frame->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum));
    connect(m_effectEdit, &EffectStackEdit::seekTimeline, this, &TransitionSettings::slotSeekTimeline);
    connect(m_effectEdit, &EffectStackEdit::importClipKeyframes, this, &TransitionSettings::prepareImportClipKeyframes);

    setEnabled(false);
    setAcceptDrops(true);

    QList<QStringList> transitionsList;
    int max = MainWindow::transitions.effectNames().count();
    QStringList transitionInfo;
    int ix = 0;
    for (; ix < max; ++ix) {
        transitionInfo = MainWindow::transitions.effectIdInfo(ix);
        if (transitionInfo.isEmpty()) {
            continue;
        }
        transitionInfo << QString::number(ix);
        transitionsList.append(transitionInfo);
    }
    ix = 0;
    for (const QStringList &value : transitionsList) {
        QStringList list = value;
        if (!list.isEmpty()) {
            list.removeLast();
        }
        transitionList->addItem(value.at(0), list);
        transitionList->setItemData(ix, MainWindow::transitions.getInfoFromIndex(value.last().toInt()), Qt::ToolTipRole);
        ++ix;
    }

    connect(transitionList, SIGNAL(activated(int)), this, SLOT(slotTransitionChanged()));
    connect(transitionTrack, SIGNAL(activated(int)), this, SLOT(slotTransitionTrackChanged()));
    connect(m_effectEdit, &EffectStackEdit::parameterChanged, this, &TransitionSettings::slotUpdateEffectParams);
}

void TransitionSettings::prepareImportClipKeyframes(GraphicsRectItem, const QMap<QString, QString> &keyframes)
{
    emit importClipKeyframes(TransitionWidget, m_usedTransition->info(), m_usedTransition->toXML(), keyframes);
}

void TransitionSettings::refreshIcons()
{
    QList<QAction *> allMenus = this->findChildren<QAction *>();
    for (int i = 0; i < allMenus.count(); i++) {
        QAction *m = allMenus.at(i);
        QIcon ic = m->icon();
        if (ic.isNull() || ic.name().isEmpty()) {
            continue;
        }
        QIcon newIcon = QIcon::fromTheme(ic.name());
        m->setIcon(newIcon);
    }
    QList<QToolButton *> allButtons = this->findChildren<QToolButton *>();
    for (int i = 0; i < allButtons.count(); i++) {
        QToolButton *m = allButtons.at(i);
        QIcon ic = m->icon();
        if (ic.isNull() || ic.name().isEmpty()) {
            continue;
        }
        QIcon newIcon = QIcon::fromTheme(ic.name());
        m->setIcon(newIcon);
    }
}

void TransitionSettings::dragEnterEvent(QDragEnterEvent *event)
{
    if (m_effectEdit->doesAcceptDrops() && event->mimeData()->hasFormat(QStringLiteral("kdenlive/geometry"))) {
        event->setDropAction(Qt::CopyAction);
        event->setAccepted(true);
    } else {
        QWidget::dragEnterEvent(event);
    }
}

void TransitionSettings::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasFormat(QStringLiteral("kdenlive/geometry"))) {
        QString itemData = QString::fromLatin1(event->mimeData()->data(QStringLiteral("kdenlive/geometry")));
        QMap<QString, QString> keyframes;
        keyframes.insert(i18n("Geometry"), itemData);
        emit importClipKeyframes(TransitionWidget, m_usedTransition->info(), m_usedTransition->toXML(), keyframes);
    }
}

void TransitionSettings::updateProjectFormat()
{
    // TODO REFAC: get tracks list
    // m_tracks = pCore->projectManager()->currentTimeline()->getTracksInfo();
    // updateTrackList();
}

void TransitionSettings::updateTimecodeFormat()
{
    m_effectEdit->updateTimecodeFormat();
}

void TransitionSettings::updateTrackList()
{
    transitionTrack->blockSignals(true);
    int current = transitionTrack->itemData(transitionTrack->currentIndex()).toInt();
    transitionTrack->clear();
    transitionTrack->addItem(i18n("Auto"), -1);
    int limit = 1;
    if (m_usedTransition) {
        limit = m_usedTransition->track() - 1;
    }
    QIcon videoIcon = QIcon::fromTheme(QStringLiteral("kdenlive-show-video"));
    QIcon audioIcon = QIcon::fromTheme(QStringLiteral("kdenlive-show-audio"));
    for (int i = limit; i > 0; i--) {
        transitionTrack->addItem(m_tracks.at(i).type == VideoTrack ? videoIcon : audioIcon,
                                 m_tracks.at(i).trackName.isEmpty() ? QString::number(i) : m_tracks.at(i).trackName, i);
    }
    transitionTrack->addItem(i18n("Black"), 0);
    transitionTrack->setCurrentIndex(transitionTrack->findData(current));
    transitionTrack->blockSignals(false);
}

void TransitionSettings::slotTransitionChanged(bool reinit, bool updateCurrent)
{
    QDomElement e = m_usedTransition->toXML().cloneNode().toElement();
    if (reinit) {
        // Reset the transition parameters to the default one
        disconnect(m_effectEdit->monitor(), &Monitor::seekPosition, this, &TransitionSettings::slotRenderPos);
        QDomElement newTransition = MainWindow::transitions.getEffectByName(transitionList->currentText()).cloneNode().toElement();
        pCore->projectManager()->currentTimeline()->transitionHandler->initTransition(newTransition);
        slotUpdateEffectParams(e, newTransition);
        m_effectEdit->transferParamDesc(newTransition, m_usedTransition->info(), false);
        if (m_effectEdit->needsMonitorEffectScene() != 0u) {
            connect(m_effectEdit->monitor(), &Monitor::seekPosition, this, &TransitionSettings::slotRenderPos, Qt::UniqueConnection);
        }
    } else if (!updateCurrent) {
        // Transition changed, update parameters dialog
        // slotUpdateEffectParams(e, e);
        m_effectEdit->transferParamDesc(e, m_usedTransition->info(), false);
    } else {
        // Same transition, we just want to update the parameters value
        int ix = transitionList->findData(m_usedTransition->transitionInfo(), Qt::UserRole, Qt::MatchExactly);
        if (ix != transitionList->currentIndex()) {
            // Transition type changed, reload params
            transitionList->blockSignals(true);
            transitionList->setCurrentIndex(ix);
            transitionList->blockSignals(false);
            m_effectEdit->transferParamDesc(e, m_usedTransition->info(), false);
        } else {
            slotUpdateEffectParams(e, e);
            if (m_usedTransition->hasGeometry()) {
                m_effectEdit->transferParamDesc(m_usedTransition->toXML(), m_usedTransition->info(), false);
            }
        }
        if (m_effectEdit->needsMonitorEffectScene() != 0u) {
            connect(m_effectEdit->monitor(), &Monitor::seekPosition, this, &TransitionSettings::slotRenderPos, Qt::UniqueConnection);
        }
    }
    // slotCheckMonitorPosition(m_effectEdit->monitor()->render->seekFramePosition());
}

void TransitionSettings::slotTransitionTrackChanged()
{
    if (m_usedTransition == nullptr) {
        return;
    }
    int ix = 0;
    QDomElement oldxml = m_usedTransition->toXML().cloneNode().toElement();
    if (transitionTrack->currentIndex() > 0) {
        ix = transitionTrack->itemData(transitionTrack->currentIndex()).toInt();
        m_usedTransition->setForcedTrack(true, ix);
        m_effectEdit->updateParameter(QStringLiteral("force_track"), QStringLiteral("1"));
    } else {
        ix = m_autoTrackTransition;
        m_usedTransition->setForcedTrack(false, ix);
        m_effectEdit->updateParameter(QStringLiteral("force_track"), QStringLiteral("0"));
    }
    emit transitionUpdated(m_usedTransition, oldxml);
    m_effectEdit->updateParameter(QStringLiteral("transition_btrack"), QString::number(ix));
}

void TransitionSettings::slotTransitionItemSelected(Transition *t, int nextTrack, const QPoint &p, bool update)
{
    setEnabled(t != nullptr);
    m_effectEdit->setFrameSize(p);
    m_autoTrackTransition = nextTrack;
    disconnect(m_effectEdit->monitor(), &Monitor::seekPosition, this, &TransitionSettings::slotRenderPos);
    if (t == m_usedTransition) {
        if (t == nullptr) {
            return;
        }
        if (update) {
            transitionTrack->blockSignals(true);
            updateTrackList();
            if (t->forcedTrack()) {
                transitionTrack->setCurrentIndex(transitionTrack->findData(t->transitionEndTrack()));
            } else {
                transitionTrack->setCurrentIndex(0);
            }
            transitionTrack->blockSignals(false);
        }
        if (update || t->cropDuration() != m_transitionDuration || t->startPos() != m_transitionStart) {
            m_transitionDuration = t->cropDuration();
            m_transitionStart = t->startPos();
            slotTransitionChanged(false, true);
        }
        if (m_effectEdit->needsMonitorEffectScene() != 0u) {
            slotRenderPos(m_effectEdit->monitor()->position());
            connect(m_effectEdit->monitor(), &Monitor::seekPosition, this, &TransitionSettings::slotRenderPos, Qt::UniqueConnection);
        }
        return;
    }
    if (update) {
        return;
    }
    if (t) {
        m_transitionDuration = t->cropDuration();
        m_transitionStart = t->startPos();
        transitionTrack->blockSignals(true);
        m_usedTransition = t;
        updateTrackList();
        if (!t->forcedTrack()) {
            transitionTrack->setCurrentIndex(0);
        } else {
            transitionTrack->setCurrentIndex(transitionTrack->findData(t->transitionEndTrack()));
        }
        transitionTrack->blockSignals(false);
        int ix = transitionList->findData(t->transitionInfo(), Qt::UserRole, Qt::MatchExactly);
        if (ix != -1) {
            transitionList->blockSignals(true);
            transitionList->setCurrentIndex(ix);
            slotTransitionChanged(false, false);
            transitionList->blockSignals(false);
        }
        if (m_effectEdit->needsMonitorEffectScene() != 0u) {
            slotRenderPos(m_effectEdit->monitor()->position());
            connect(m_effectEdit->monitor(), &Monitor::seekPosition, this, &TransitionSettings::slotRenderPos, Qt::UniqueConnection);
        }
    } else {
        // null transition selected
        m_usedTransition = nullptr;
        ItemInfo info;
        m_effectEdit->transferParamDesc(QDomElement(), info, false);
        m_effectEdit->monitor()->slotShowEffectScene(MonitorSceneDefault);
    }
}

void TransitionSettings::slotUpdateEffectParams(const QDomElement &oldparam, const QDomElement &param)
{
    if (m_usedTransition) {
        m_usedTransition->setTransitionParameters(param);
    }
    // oldparam must be also first given to Transition and then return the toXML()
    if (oldparam != param) {
        emit transitionUpdated(m_usedTransition, oldparam);
    }
}

void TransitionSettings::raiseWindow(QWidget *dock)
{
    if ((dock != nullptr) && (m_usedTransition != nullptr)) {
        dock->raise();
    }
}

void TransitionSettings::slotRenderPos(int pos)
{
    if (m_usedTransition) {
        m_effectEdit->slotSyncEffectsPos(pos - m_usedTransition->startPos().frames(KdenliveSettings::project_fps()));
        if (isEnabled()) {
            slotCheckMonitorPosition(pos);
        }
    }
}

void TransitionSettings::slotSeekTimeline(int pos)
{
    if (m_usedTransition) {
        emit seekTimeline(m_usedTransition->startPos().frames(KdenliveSettings::project_fps()) + pos);
    }
}

void TransitionSettings::slotCheckMonitorPosition(int renderPos)
{
    if (!isEnabled()) {
        return;
    }
    MonitorSceneType sceneType = m_effectEdit->needsMonitorEffectScene();
    if (sceneType != MonitorSceneDefault) {
        if (renderPos >= m_usedTransition->startPos().frames(KdenliveSettings::project_fps()) &&
            renderPos < m_usedTransition->endPos().frames(KdenliveSettings::project_fps())) {
            if (!m_effectEdit->monitor()->effectSceneDisplayed(sceneType)) {
                m_effectEdit->monitor()->slotShowEffectScene(sceneType);
                m_effectEdit->initEffectScene(renderPos - m_usedTransition->startPos().frames(KdenliveSettings::project_fps()));
            }
        } else {
            m_effectEdit->monitor()->slotShowEffectScene(MonitorSceneDefault);
        }
    } else {
        m_effectEdit->monitor()->slotShowEffectScene(MonitorSceneDefault);
    }
}

void TransitionSettings::setKeyframes(const QString &tag, const QString &keyframes)
{
    m_effectEdit->setKeyframes(tag, keyframes);
}

void TransitionSettings::updatePalette()
{
    // We need to reset current stylesheet if we want to change the palette!
    m_effectEdit->updatePalette();
}
