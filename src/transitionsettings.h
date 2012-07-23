/***************************************************************************
                          transitionsettings.h  -  Transitions widget
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
class Monitor;

class TransitionSettings : public QWidget, public Ui::TransitionSettings_UI
{
    Q_OBJECT

public:
    explicit TransitionSettings(Monitor *monitor, QWidget* parent = 0);
    void raiseWindow(QWidget*);
    void updateProjectFormat(MltVideoProfile profile, Timecode t, const QList <TrackInfo> info);
    void updateTimecodeFormat();

private:
    EffectStackEdit *m_effectEdit;
    Transition* m_usedTransition;
    GenTime m_transitionDuration;
    GenTime m_transitionStart;
    int m_autoTrackTransition;
    QList <TrackInfo> m_tracks;
    void updateTrackList();

public slots:
    void slotTransitionItemSelected(Transition* t, int nextTrack, QPoint p, bool update);
    void slotTransitionChanged(bool reinit = true, bool updateCurrent = false);
    void slotUpdateEffectParams(const QDomElement, const QDomElement);

private slots:
    /** @brief Sets the new B track for the transition (automatic or forced). */
    void slotTransitionTrackChanged();
    /** @brief Pass position changes of the timeline cursor to the effects to keep their local timelines in sync. */
    void slotRenderPos(int pos);
    void slotSeekTimeline(int pos);
    void slotCheckMonitorPosition(int renderPos);

signals:
    void transitionUpdated(Transition *, QDomElement);
    void seekTimeline(int);
};

#endif
