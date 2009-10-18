/***************************************************************************
                          effecstackedit.h  -  description
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

#include <KDebug>

TransitionSettings::TransitionSettings(QWidget* parent) :
        QWidget(parent),
        m_usedTransition(NULL),
        m_tracksCount(0),
        m_autoTrackTransition(0)
{
    setupUi(this);
    QVBoxLayout *vbox1 = new QVBoxLayout(frame);
    m_effectEdit = new EffectStackEdit(frame);
    vbox1->setContentsMargins(0, 0, 0, 0);
    vbox1->setSpacing(0);
    vbox1->addWidget(m_effectEdit);
    frame->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum));
    connect(m_effectEdit, SIGNAL(seekTimeline(int)), this, SIGNAL(seekTimeline(int)));
    setEnabled(false);

    QMap<QString, QStringList> transitionsList;
    int max = MainWindow::transitions.effectNames().count();
    QStringList transitionInfo;
    int ix = 0;

    for (; ix < max; ix++) {
        transitionInfo = MainWindow::transitions.effectIdInfo(ix);
        transitionInfo << QString::number(ix);
        transitionsList.insert(transitionInfo.at(0).toLower(), transitionInfo);
    }
    ix = 0;
    foreach(const QStringList &value, transitionsList) {
        transitionList->addItem(value.at(0));
        transitionList->setItemData(ix, MainWindow::transitions.getInfoFromIndex(value.last().toInt()), Qt::ToolTipRole);
        ix++;
    }

    connect(transitionList, SIGNAL(activated(int)), this, SLOT(slotTransitionChanged()));
    connect(transitionTrack, SIGNAL(activated(int)), this, SLOT(slotTransitionTrackChanged()));

    connect(this, SIGNAL(transferParamDesc(const QDomElement&, int , int)), m_effectEdit , SLOT(transferParamDesc(const QDomElement&, int , int)));
    connect(m_effectEdit, SIGNAL(parameterChanged(const QDomElement&, const QDomElement&)), this , SLOT(slotUpdateEffectParams(const QDomElement&, const QDomElement&)));
}

void TransitionSettings::updateProjectFormat(MltVideoProfile profile, Timecode t, const QList <TrackInfo> info)
{
    m_tracksCount = info.count();
    m_effectEdit->updateProjectFormat(profile, t);
    QStringList tracksList;
    transitionTrack->blockSignals(true);
    transitionTrack->clear();
    transitionTrack->addItem(i18n("Auto"), -1);
    for (int i = 0; i < m_tracksCount; i++) {
        if (!info.at(i).trackName.isEmpty())
            transitionTrack->addItem(info.at(i).trackName + '(' + QString::number(i) + ')', i);
        else transitionTrack->addItem(QString::number(i));
    }
    transitionTrack->addItem(i18n("Black"), m_tracksCount);
    transitionTrack->blockSignals(false);
}


void TransitionSettings::slotTransitionChanged(bool reinit, bool updateCurrent)
{
    QDomElement e = m_usedTransition->toXML().cloneNode().toElement();
    if (reinit) {
        // Reset the transition parameters to the default one
        QDomElement newTransition = MainWindow::transitions.getEffectByName(transitionList->currentText()).cloneNode().toElement();
        slotUpdateEffectParams(e, newTransition);
        emit transferParamDesc(newTransition, m_usedTransition->startPos().frames(KdenliveSettings::project_fps()), m_usedTransition->endPos().frames(KdenliveSettings::project_fps()));
    } else if (!updateCurrent) {
        // Transition changed, update parameters dialog
        //slotUpdateEffectParams(e, e);
        m_effectEdit->transferParamDesc(e, m_usedTransition->startPos().frames(KdenliveSettings::project_fps()), m_usedTransition->endPos().frames(KdenliveSettings::project_fps()));
    } else {
        // Same transition, we just want to update the parameters value
        slotUpdateEffectParams(e, e);
        if (m_usedTransition->hasGeometry()) emit transferParamDesc(m_usedTransition->toXML(), m_usedTransition->startPos().frames(KdenliveSettings::project_fps()), m_usedTransition->endPos().frames(KdenliveSettings::project_fps()));
    }
}

void TransitionSettings::slotTransitionTrackChanged()
{
    if (m_usedTransition == NULL) return;
    int ix = 0;
    QDomElement oldxml = m_usedTransition->toXML().cloneNode().toElement();
    if (transitionTrack->currentIndex() > 0) {
        ix = transitionTrack->count() - transitionTrack->currentIndex() - 1;
        m_usedTransition->setForcedTrack(true, ix);
        m_effectEdit->updateParameter("force_track", "1");
        emit transitionUpdated(m_usedTransition, oldxml);
    } else {
        ix = m_autoTrackTransition;
        m_usedTransition->setForcedTrack(false, ix);
        m_effectEdit->updateParameter("force_track", "0");
        emit transitionUpdated(m_usedTransition, oldxml);
    }
    m_effectEdit->updateParameter("transition_btrack", QString::number(ix));
}

void TransitionSettings::slotTransitionItemSelected(Transition* t, int nextTrack, QPoint p, bool update)
{
    setEnabled(t != NULL);
    m_effectEdit->setFrameSize(p);
    m_autoTrackTransition = nextTrack;
    if (t == m_usedTransition) {
        if (t == NULL) return;
        if (update) {
            transitionTrack->blockSignals(true);
            if (t->forcedTrack()) transitionTrack->setCurrentIndex(m_tracksCount + 1 - t->transitionEndTrack());
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
        if (!t->forcedTrack()) transitionTrack->setCurrentIndex(0);
        else transitionTrack->setCurrentIndex(m_tracksCount + 1 - t->transitionEndTrack());
        transitionTrack->blockSignals(false);
        int ix = transitionList->findText(t->transitionName(), Qt::MatchExactly);
        m_usedTransition = t;
        if (ix != -1) {
            transitionList->blockSignals(true);
            transitionList->setCurrentIndex(ix);
            slotTransitionChanged(false, false);
            transitionList->blockSignals(false);
        }
    } else {
        // null transition selected
        m_usedTransition = NULL;
        m_effectEdit->transferParamDesc(QDomElement(), 0, 0);
    }

}

void TransitionSettings::slotUpdateEffectParams(const QDomElement &oldparam, const QDomElement &param)
{
    if (m_usedTransition) {
        m_usedTransition->setTransitionParameters(param);
        m_usedTransition->update();
    }
    //oldparam must be also first given to Transition and then return the toXML()
    if (oldparam != param) {
        emit transitionUpdated(m_usedTransition, oldparam);
    }
}

void TransitionSettings::raiseWindow(QWidget* dock)
{
    if (dock && m_usedTransition)
        dock->raise();

}

