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

#include <QDomElement>

#include "ui_transitionsettings_ui.h"
#include "definitions.h"

class Timecode;
class Transition;
class EffectsList;
class EffectStackEdit;

class TransitionSettings : public QWidget
{
    Q_OBJECT

public:
    TransitionSettings(QWidget* parent = 0);
    void raiseWindow(QWidget*);
    void updateProjectFormat(MltVideoProfile profile, Timecode t, const uint tracksCount);

private:
    Ui::TransitionSettings_UI m_ui;
    EffectStackEdit *m_effectEdit;
    Transition* m_usedTransition;
    GenTime m_transitionDuration;
    GenTime m_transitionStart;
    int m_tracksCount;
    int m_autoTrackTransition;

public slots:
    void slotTransitionItemSelected(Transition* t, int nextTrack, QPoint p, bool update);
    void slotTransitionChanged(bool reinit = true, bool updateCurrent = false);
    void slotUpdateEffectParams(const QDomElement&, const QDomElement&);

private slots:
    void slotTransitionTrackChanged();

signals:
    void transitionUpdated(Transition *, QDomElement);
    void transferParamDesc(const QDomElement&, int , int);
    void seekTimeline(int);
};

#endif
