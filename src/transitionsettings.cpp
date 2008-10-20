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

TransitionSettings::TransitionSettings(QWidget* parent): QWidget(parent) {
    ui.setupUi(this);
    effectEdit = new EffectStackEdit(ui.frame);
    connect(effectEdit, SIGNAL(seekTimeline(int)), this, SIGNAL(seekTimeline(int)));
    setEnabled(false);
    ui.transitionList->addItems(MainWindow::transitions.effectNames());
    //kDebug() << MainWindow::transitions.effectNames().size();
    //ui.listWidget->setCurrentRow(0);
    connect(ui.transitionList, SIGNAL(activated(int)), this, SLOT(slotTransitionChanged()));
    connect(this, SIGNAL(transferParamDesc(const QDomElement&, int , int)), effectEdit , SLOT(transferParamDesc(const QDomElement&, int , int)));
    connect(effectEdit, SIGNAL(parameterChanged(const QDomElement&, const QDomElement&)), this , SLOT(slotUpdateEffectParams(const QDomElement&, const QDomElement&)));
    ui.splitter->setStretchFactor(0, 1);
    ui.splitter->setStretchFactor(1, 10);
}

void TransitionSettings::updateProjectFormat(MltVideoProfile profile) {
    effectEdit->updateProjectFormat(profile);
}


void TransitionSettings::slotTransitionChanged(bool reinit) {
    QDomElement e = m_usedTransition->toXML().cloneNode().toElement();
    if (reinit) {
        QDomElement newTransition = MainWindow::transitions.getEffectByName(ui.transitionList->currentText());
        slotUpdateEffectParams(e, newTransition);
        emit transferParamDesc(newTransition, m_usedTransition->startPos().frames(KdenliveSettings::project_fps()), m_usedTransition->endPos().frames(KdenliveSettings::project_fps()));
    } else {
        //slotUpdateEffectParams(e, e);
        emit transferParamDesc(e, m_usedTransition->startPos().frames(KdenliveSettings::project_fps()), m_usedTransition->endPos().frames(KdenliveSettings::project_fps()));
    }
}

void TransitionSettings::slotTransitionItemSelected(Transition* t) {
    setEnabled(t != NULL);
    if (t == m_usedTransition) {
        if (t && (t->duration() != m_transitionDuration || t->startPos() != m_transitionStart)) {
            m_transitionDuration = t->duration();
            m_transitionStart = t->startPos();
            slotTransitionChanged(false);
        }
        return;
    }
    m_usedTransition = t;
    if (m_usedTransition) {
        m_transitionDuration = t->duration();
        m_transitionStart = t->startPos();
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

