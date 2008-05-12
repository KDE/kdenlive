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
#include "mainwindow.h"

TransitionSettings::TransitionSettings(QWidget* parent): QWidget(parent) {
    ui.setupUi(this);
    effectEdit = new EffectStackEdit(ui.frame, this);
    setEnabled(false);
    ui.listWidget->addItems(MainWindow::transitions.effectNames());
    kDebug() << MainWindow::transitions.effectNames().size();
    ui.listWidget->setCurrentRow(0);
    connect(ui.listWidget, SIGNAL(currentRowChanged(int)), this, SLOT(slotTransitionChanged()));
    connect(this, SIGNAL(transferParamDesc(const QDomElement&, int , int)), effectEdit , SLOT(transferParamDesc(const QDomElement&, int , int)));
    connect(effectEdit, SIGNAL(parameterChanged(const QDomElement&, const QDomElement&)), this , SLOT(slotUpdateEffectParams(const QDomElement&, const QDomElement&)));
}


void TransitionSettings::slotTransitionChanged() {

    QDomElement e = m_usedTransition->toXML();

    //set old values from e in <ktransition> to desc (like reverse and so )
    /* QDomElement desc = m_transitions->getEffectByName(ui.listWidget->currentItem()->text());
     if (m_usedTransition) {
         m_usedTransition->setTransitionParameters(desc);
         m_usedTransition->update();
     }
     emit transitionUpdated(e, m_usedTransition->toXML());*/
    /*QDomDocument doc;    
    doc.appendChild(doc.importNode(e, true));
    kDebug()<<"///////////  TRANSITION CHANGED: "<<doc.toString();
    kDebug()<<"///////////  TRANSITION CHANGED END...";*/

    if (m_usedTransition && m_usedTransition->transitionName() == ui.listWidget->currentItem()->text() && !e.attribute("tag").isNull()) {
        slotUpdateEffectParams(e, e);
    } else
        slotUpdateEffectParams(e, MainWindow::transitions.getEffectByName(ui.listWidget->currentItem()->text()));
    emit transferParamDesc(e, 0, 0);
}

void TransitionSettings::slotTransitionItemSelected(Transition* t) {
    setEnabled(t != NULL);
    if (t == m_usedTransition) return;
    m_usedTransition = t;
    if (m_usedTransition) {
        QList<QListWidgetItem*> list = ui.listWidget->findItems(m_usedTransition->transitionName(), Qt::MatchExactly);
        if (list.size() > 0) {
            ui.listWidget->blockSignals(true);
            ui.listWidget->setCurrentItem(list[0]);
            slotTransitionChanged();
            ui.listWidget->blockSignals(false);
        }
    }

}

void TransitionSettings::slotUpdateEffectParams(const QDomElement& oldparam, const QDomElement& param) {
    if (m_usedTransition) {
        m_usedTransition->setTransitionParameters(param);
        m_usedTransition->update();
    }
    QString test;
    QTextStream str(&test);
    oldparam.save(str, 2);
    m_usedTransition->toXML().save(str, 2);
    //kDebug() << test;
    //oldparam must be also first given to Transition and then return the toXML()
    emit transitionUpdated(m_usedTransition, oldparam);
}

void TransitionSettings::raiseWindow(QWidget* dock) {
    if (dock && m_usedTransition)
        dock->raise();

}

