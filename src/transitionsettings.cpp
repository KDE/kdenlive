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
#include <KDebug>

TransitionSettings::TransitionSettings(EffectsList *transitions, QWidget* parent): QWidget(parent) {
    ui.setupUi(this);
    setEnabled(false);
    m_transitions = transitions;
    ui.listWidget->addItems(transitions->effectNames());
    kDebug() << transitions->effectNames().size() << " -" << transitions->size();
}

void TransitionSettings::slotTransitionItemSelected(Transition* t) {
    setEnabled(t != NULL);
}

