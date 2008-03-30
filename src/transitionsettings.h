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
#ifndef TRANSITIONSETTINGS_H
#define TRANSITIONSETTINGS_H
#include "ui_transitionsettings_ui.h"
#include <QDomElement>

class Transition;
class EffectsList;
class EffectStackEdit;

class TransitionSettings : public QWidget  {
    Q_OBJECT
public:
    TransitionSettings(QWidget* parent = 0);
    void raiseWindow(QWidget*);
private:
    Ui::TransitionSettings_UI ui;
    EffectStackEdit *effectEdit;
    Transition* m_usedTransition;
public slots:
    void slotTransitionItemSelected(Transition*);
    void slotTransitionChanged();
    void slotUpdateEffectParams(const QDomElement&, const QDomElement&);
signals:
    void transitionUpdated(Transition *, QDomElement);
    void transferParamDesc(const QDomElement&, int , int);
};

#endif
