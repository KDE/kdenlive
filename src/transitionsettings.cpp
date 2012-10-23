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
#include "transition.h"
#include "effectslist.h"
#include "effectstackedit.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "monitoreditwidget.h"
#include "monitorscene.h"

#include <KDebug>

TransitionSettings::TransitionSettings(Monitor *monitor, QWidget* parent) :
        QWidget(parent),
        m_usedTransition(NULL),
        m_autoTrackTransition(0)
{
    setupUi(this);
    QVBoxLayout *vbox1 = new QVBoxLayout(frame);
    m_effectEdit = new EffectStackEdit(monitor, frame);
    vbox1->setContentsMargins(0, 0, 0, 0);
    vbox1->setSpacing(0);
    vbox1->addWidget(m_effectEdit);
    frame->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum));
    connect(m_effectEdit, SIGNAL(seekTimeline(int)), this, SLOT(slotSeekTimeline(int)));
    connect(m_effectEdit, SIGNAL(importClipKeyframes()), this, SIGNAL(importClipKeyframes()));
    
    setEnabled(false);

    QList<QStringList> transitionsList;
    int max = MainWindow::transitions.effectNames().count();
    QStringList transitionInfo;
    int ix = 0;

    for (; ix < max; ix++) {
        transitionInfo = MainWindow::transitions.effectIdInfo(ix);
        transitionInfo << QString::number(ix);
        transitionsList.append(transitionInfo);
    }
    ix = 0;
    foreach(const QStringList &value, transitionsList) {
        QStringList data = value;
        if (!data.isEmpty()) data.removeLast();
        transitionList->addItem(value.at(0), data);
        transitionList->setItemData(ix, MainWindow::transitions.getInfoFromIndex(value.last().toInt()), Qt::ToolTipRole);
        ix++;
    }

    connect(transitionList, SIGNAL(activated(int)), this, SLOT(slotTransitionChanged()));
    connect(transitionTrack, SIGNAL(activated(int)), this, SLOT(slotTransitionTrackChanged()));
    connect(m_effectEdit, SIGNAL(parameterChanged(const QDomElement, const QDomElement, int)), this , SLOT(slotUpdateEffectParams(const QDomElement, const QDomElement)));
}

void TransitionSettings::updateProjectFormat(MltVideoProfile profile, Timecode t, const QList <TrackInfo> info)
{
    m_effectEdit->updateProjectFormat(profile, t);
    m_tracks = info;
    updateTrackList();
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
    if (m_usedTransition)
        limit = m_usedTransition->track() + 1;
    //kDebug() << "/ / TRANS TRK: " << limit;
    KIcon videoIcon("kdenlive-show-video");
    KIcon audioIcon("kdenlive-show-audio");
    for (int i = limit; i < m_tracks.count(); i++) {
        int ix = m_tracks.count() - i - 1;
        transitionTrack->addItem(m_tracks.at(ix).type == VIDEOTRACK ? videoIcon : audioIcon,
                                 m_tracks.at(ix).trackName.isEmpty() ? QString::number(i) : m_tracks.at(ix).trackName + " (" + QString::number(i) + ')',
                                 m_tracks.count() - i);
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
        disconnect(m_effectEdit->monitor(), SIGNAL(renderPosition(int)), this, SLOT(slotRenderPos(int)));
        QDomElement newTransition = MainWindow::transitions.getEffectByName(transitionList->currentText()).cloneNode().toElement();
        slotUpdateEffectParams(e, newTransition);
        m_effectEdit->transferParamDesc(newTransition, m_usedTransition->info(), false);
	if (m_effectEdit->needsMonitorEffectScene())
	    connect(m_effectEdit->monitor(), SIGNAL(renderPosition(int)), this, SLOT(slotRenderPos(int)));
    } else if (!updateCurrent) {
        // Transition changed, update parameters dialog
        //slotUpdateEffectParams(e, e);
        m_effectEdit->transferParamDesc(e, m_usedTransition->info(), false);
    } else {
        // Same transition, we just want to update the parameters value
        slotUpdateEffectParams(e, e);
        if (m_usedTransition->hasGeometry())
            m_effectEdit->transferParamDesc(m_usedTransition->toXML(), m_usedTransition->info(), false);
    }
    slotCheckMonitorPosition(m_effectEdit->monitor()->render->seekFramePosition());
}

void TransitionSettings::slotTransitionTrackChanged()
{
    if (m_usedTransition == NULL) return;
    int ix = 0;
    QDomElement oldxml = m_usedTransition->toXML().cloneNode().toElement();
    if (transitionTrack->currentIndex() > 0) {
        ix = transitionTrack->itemData(transitionTrack->currentIndex()).toInt();
        m_usedTransition->setForcedTrack(true, ix);
        m_effectEdit->updateParameter("force_track", "1");
    } else {
        ix = m_autoTrackTransition;
        m_usedTransition->setForcedTrack(false, ix);
        m_effectEdit->updateParameter("force_track", "0");
    }
    emit transitionUpdated(m_usedTransition, oldxml);
    m_effectEdit->updateParameter("transition_btrack", QString::number(ix));
}

void TransitionSettings::slotTransitionItemSelected(Transition* t, int nextTrack, QPoint p, bool update)
{
    setEnabled(t != NULL);
    m_effectEdit->setFrameSize(p);
    m_autoTrackTransition = nextTrack;
    disconnect(m_effectEdit->monitor(), SIGNAL(renderPosition(int)), this, SLOT(slotRenderPos(int)));
    if (t == m_usedTransition) {
        if (t == NULL) return;
        if (update) {
            transitionTrack->blockSignals(true);
            updateTrackList();
            if (t->forcedTrack()) transitionTrack->setCurrentIndex(transitionTrack->findData(t->transitionEndTrack()));
            else transitionTrack->setCurrentIndex(0);
            transitionTrack->blockSignals(false);
        }
        if (update || t->cropDuration() != m_transitionDuration || t->startPos() != m_transitionStart) {
            m_transitionDuration = t->cropDuration();
            m_transitionStart = t->startPos();
            slotTransitionChanged(false, true);
        }
        return;
    } else if (update) return;
    if (t) {
        m_transitionDuration = t->cropDuration();
        m_transitionStart = t->startPos();
        transitionTrack->blockSignals(true);
        m_usedTransition = t;
        updateTrackList();
        if (!t->forcedTrack())
            transitionTrack->setCurrentIndex(0);
        else
            transitionTrack->setCurrentIndex(transitionTrack->findData(t->transitionEndTrack()));
        transitionTrack->blockSignals(false);
        int ix = transitionList->findData(t->transitionInfo(), Qt::UserRole, Qt::MatchExactly);
        if (ix != -1) {
            transitionList->blockSignals(true);
            transitionList->setCurrentIndex(ix);
            slotTransitionChanged(false, false);
            transitionList->blockSignals(false);
        }
        if (m_effectEdit->needsMonitorEffectScene()) {
	    connect(m_effectEdit->monitor(), SIGNAL(renderPosition(int)), this, SLOT(slotRenderPos(int)));
	}
    } else {
        // null transition selected
        m_usedTransition = NULL;
        ItemInfo info;
        m_effectEdit->transferParamDesc(QDomElement(), info, false);
    }
}

void TransitionSettings::slotUpdateEffectParams(const QDomElement oldparam, const QDomElement param)
{
    if (m_usedTransition) {
        m_usedTransition->setTransitionParameters(param);
        m_usedTransition->update();
    }
    //oldparam must be also first given to Transition and then return the toXML()
    if (oldparam != param)
        emit transitionUpdated(m_usedTransition, oldparam);
}

void TransitionSettings::raiseWindow(QWidget* dock)
{
    if (dock && m_usedTransition)
        dock->raise();
}

void TransitionSettings::slotRenderPos(int pos)
{
    if (m_usedTransition) {
        m_effectEdit->slotSyncEffectsPos(pos - m_usedTransition->startPos().frames(KdenliveSettings::project_fps()));
	if (isEnabled()) slotCheckMonitorPosition(pos);
    }
}

void TransitionSettings::slotSeekTimeline(int pos)
{
    if (m_usedTransition)
	emit seekTimeline(m_usedTransition->startPos().frames(KdenliveSettings::project_fps()) + pos);
}

void TransitionSettings::slotCheckMonitorPosition(int renderPos)
{
    if (!isEnabled()) return;
    if (m_effectEdit->needsMonitorEffectScene()) {
	if (renderPos >= m_usedTransition->startPos().frames(KdenliveSettings::project_fps()) && renderPos <= m_usedTransition->endPos().frames(KdenliveSettings::project_fps())) {
	    if (!m_effectEdit->monitor()->effectSceneDisplayed()) {
		m_effectEdit->monitor()->slotShowEffectScene(true);
	    }
	} else {
	    m_effectEdit->monitor()->slotShowEffectScene(false);
	}
    }
    else {
	m_effectEdit->monitor()->slotShowEffectScene(false);
    }
}

void TransitionSettings::setKeyframes(const QString data)
{
    m_effectEdit->setKeyframes(data);
}

#include "transitionsettings.moc"
