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
#include <KDebug>
#include "effectslist.h"
#include "effectstackedit.h"

TransitionSettings::TransitionSettings(EffectsList *transitions, QWidget* parent): QWidget(parent) {
    ui.setupUi(this);
    effectEdit = new EffectStackEdit(ui.frame, this);
    setEnabled(false);
    m_transitions = transitions;
    ui.listWidget->addItems(transitions->effectNames());
    kDebug() << transitions->effectNames().size() << " -" << transitions->size();
    connect(ui.listWidget, SIGNAL(currentRowChanged(int)), this, SLOT(slotTransitionChanged()));
    connect(this, SIGNAL(transferParamDesc(const QDomElement&, int , int)), effectEdit , SLOT(transferParamDesc(const QDomElement&, int , int)));
    connect(effectEdit, SIGNAL(parameterChanged(const QDomElement&, const QDomElement&)), this , SLOT(slotUpdateEffectParams(const QDomElement&, const QDomElement&)));
}


void TransitionSettings::slotTransitionChanged() {
    QDomElement e = m_usedTransition->toXML();
    QDomElement newElement = e.cloneNode().toElement();

    QDomElement desc = m_transitions->getEffectByName(ui.listWidget->currentItem()->text());

    newElement.setAttribute("type", desc.attribute("tag"));
    newElement.setAttribute("invert", "1");

    emit transitionUpdated(e, newElement);
    emit transferParamDesc(desc, 0, 0);
}

void TransitionSettings::slotTransitionItemSelected(Transition* t) {
    setEnabled(t != NULL);
    m_usedTransition = t;
}

void TransitionSettings::slotUpdateEffectParams(const QDomElement& oldparam, const QDomElement& param) {

    //emit transitionUpdated(oldparam, param);
}