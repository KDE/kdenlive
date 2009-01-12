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


#include <KDebug>

#include "transitionsettings.h"
#include "transition.h"
#include "effectslist.h"
#include "effectstackedit.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"

TransitionSettings::TransitionSettings(QWidget* parent): QWidget(parent), m_tracksCount(0) {
    ui.setupUi(this);
    effectEdit = new EffectStackEdit(ui.frame);
    connect(effectEdit, SIGNAL(seekTimeline(int)), this, SIGNAL(seekTimeline(int)));
    setEnabled(false);
    ui.transitionList->addItems(MainWindow::transitions.effectNames());
    for (int i = 0; i < ui.transitionList->count(); i++) {
        ui.transitionList->setItemData(i, MainWindow::transitions.getInfoFromIndex(i), Qt::ToolTipRole);
    }
    //kDebug() << MainWindow::transitions.effectNames().size();
    //ui.listWidget->setCurrentRow(0);
    connect(ui.transitionList, SIGNAL(activated(int)), this, SLOT(slotTransitionChanged()));
    connect(ui.transitionTrack, SIGNAL(activated(int)), this, SLOT(slotTransitionTrackChanged()));

    connect(this, SIGNAL(transferParamDesc(const QDomElement&, int , int)), effectEdit , SLOT(transferParamDesc(const QDomElement&, int , int)));
    connect(effectEdit, SIGNAL(parameterChanged(const QDomElement&, const QDomElement&)), this , SLOT(slotUpdateEffectParams(const QDomElement&, const QDomElement&)));
}

void TransitionSettings::updateProjectFormat(MltVideoProfile profile, Timecode t, const uint tracksCount) {
    m_tracksCount = tracksCount;
    effectEdit->updateProjectFormat(profile, t);
    QStringList tracksList;
    tracksList << i18n("Auto");
    for (uint i = 0; i < tracksCount; i++) {
        tracksList << QString::number(i);
    }
    tracksList << i18n("Black");
    ui.transitionTrack->blockSignals(true);
    ui.transitionTrack->clear();
    ui.transitionTrack->addItems(tracksList);
    ui.transitionTrack->blockSignals(false);
}


void TransitionSettings::slotTransitionChanged(bool reinit) {
    QDomElement e = m_usedTransition->toXML().cloneNode().toElement();
    if (reinit) {
        QDomElement newTransition = MainWindow::transitions.getEffectByName(ui.transitionList->currentText()).cloneNode().toElement();
        slotUpdateEffectParams(e, newTransition);
        emit transferParamDesc(newTransition, m_usedTransition->startPos().frames(KdenliveSettings::project_fps()), m_usedTransition->endPos().frames(KdenliveSettings::project_fps()));
    } else {
        //slotUpdateEffectParams(e, e);
        effectEdit->transferParamDesc(e, m_usedTransition->startPos().frames(KdenliveSettings::project_fps()), m_usedTransition->endPos().frames(KdenliveSettings::project_fps()));
    }
}

void TransitionSettings::slotTransitionTrackChanged() {
    emit transitionTrackUpdated(m_usedTransition, ui.transitionTrack->currentIndex());
}

void TransitionSettings::slotTransitionItemSelected(Transition* t, bool update) {
    setEnabled(t != NULL);
    if (t == m_usedTransition) {
        if (t == NULL) return;
        if (update) {
            ui.transitionTrack->blockSignals(true);
            if (t->forcedTrack()) ui.transitionTrack->setCurrentIndex(m_tracksCount + 1 - t->transitionEndTrack());
            else ui.transitionTrack->setCurrentIndex(0);
            ui.transitionTrack->blockSignals(false);
        }
        if (update || t->duration() != m_transitionDuration || t->startPos() != m_transitionStart) {
            m_transitionDuration = t->duration();
            m_transitionStart = t->startPos();
            slotTransitionChanged(false);
        }
        return;
    } else if (update) return;
    m_usedTransition = t;
    if (m_usedTransition) {
        m_transitionDuration = t->duration();
        m_transitionStart = t->startPos();
        ui.transitionTrack->blockSignals(true);
        if (!t->forcedTrack()) ui.transitionTrack->setCurrentIndex(0);
        else ui.transitionTrack->setCurrentIndex(m_tracksCount + 1 - t->transitionEndTrack());
        ui.transitionTrack->blockSignals(false);
        int ix = ui.transitionList->findText(m_usedTransition->transitionName(), Qt::MatchExactly);
        if (ix != -1) {
            ui.transitionList->blockSignals(true);
            ui.transitionList->setCurrentIndex(ix);
            slotTransitionChanged(false);
            ui.transitionList->blockSignals(false);
        }
    }

}

void TransitionSettings::slotUpdateEffectParams(const QDomElement& oldparam, const QDomElement& param) {
    if (m_usedTransition) {
        m_usedTransition->setTransitionParameters(param);
        m_usedTransition->update();
    }
    /*QString test;
    QTextStream str(&test);
    oldparam.save(str, 2);
    m_usedTransition->toXML().save(str, 2);*/
    //kDebug() << test;
    //oldparam must be also first given to Transition and then return the toXML()
    if (oldparam != param) emit transitionUpdated(m_usedTransition, oldparam);
}

void TransitionSettings::raiseWindow(QWidget* dock) {
    if (dock && m_usedTransition)
        dock->raise();

}

