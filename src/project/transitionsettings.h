/***************************************************************************
                          project/transitionsettings.h  -  Transitions widget
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

#include "definitions.h"
#include "ui_transitionsettings_ui.h"

class Timecode;
class Transition;
class EffectStackEdit;
class Monitor;

class TransitionSettings : public QWidget, public Ui::TransitionSettings_UI
{
    Q_OBJECT

public:
    explicit TransitionSettings(Monitor *monitor, QWidget *parent = nullptr);
    void raiseWindow(QWidget *);
    void updateProjectFormat();
    void updateTimecodeFormat();
    void setKeyframes(const QString &tag, const QString &data);
    void updatePalette();
    void refreshIcons();

private:
    EffectStackEdit *m_effectEdit;
    Transition *m_usedTransition;
    GenTime m_transitionDuration;
    GenTime m_transitionStart;
    int m_autoTrackTransition;
    QList<TrackInfo> m_tracks;
    void updateTrackList();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

public slots:
    void slotTransitionItemSelected(Transition *t, int nextTrack, const QPoint &p, bool update);
    void slotTransitionChanged(bool reinit = true, bool updateCurrent = false);
    void slotUpdateEffectParams(const QDomElement &, const QDomElement &);

private slots:
    /** @brief Sets the new B track for the transition (automatic or forced). */
    void slotTransitionTrackChanged();
    /** @brief Pass position changes of the timeline cursor to the effects to keep their local timelines in sync. */
    void slotRenderPos(int pos);
    void slotSeekTimeline(int pos);
    void slotCheckMonitorPosition(int renderPos);
    void prepareImportClipKeyframes(GraphicsRectItem, const QMap<QString, QString> &keyframes);

signals:
    void transitionUpdated(Transition *, const QDomElement &);
    void seekTimeline(int);
    void importClipKeyframes(GraphicsRectItem, const ItemInfo &, const QDomElement &, const QMap<QString, QString> &keyframes = QMap<QString, QString>());
};

#endif
