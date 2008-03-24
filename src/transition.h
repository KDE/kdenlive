/***************************************************************************
                          transition.h  -  description
                             -------------------
    begin                : Tue Jan 24 2006
    copyright            : (C) 2006 by Jean-Baptiste Mardelle
    email                : jb@ader.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef TRANSITION_H
#define TRANSITION_H

#include <QString>
#include <QGraphicsRectItem>
#include <QPixmap>
#include <qdom.h>
#include <QMap>

#include "gentime.h"
#include "definitions.h"
#include "abstractclipitem.h"

/**Describes a Transition, with a name, parameters keyframes, etc.
  *@author Jean-Baptiste Mardelle
  */

class ClipItem;

class Transition : public AbstractClipItem {
    Q_OBJECT
public:

    Transition(const QRectF&, ClipItem * clipa, const QString& type, const GenTime &startTime, const GenTime &endTime, double fps, bool inverted = false);
    Transition(const QRectF&, ClipItem * clip, QDomElement transitionElement, double fps, GenTime offset = GenTime());
    virtual ~Transition();
    virtual void paint(QPainter *painter,
                       const QStyleOptionGraphicsItem *option,
                       QWidget *widget);
    virtual int type() const;
    /** Returns an XML representation of this transition. */
    QDomElement toXML();

    GenTime transitionStartTime() const;
    GenTime transitionEndTime() const;
    /** Return the track number of transition in the playlist*/
    int transitionStartTrack() const;
    int transitionEndTrack() const;
    Transition *clone();
    bool hasClip(const ClipItem * clip) const;
    bool belongsToClip(const ClipItem * clip) const;
    void resizeTransitionEnd(GenTime time);
    void resizeTransitionStart(GenTime time);
    void moveTransition(GenTime time);
    bool invertTransition() const;
    QString transitionName() const;
    OPERATIONTYPE operationMode(QPointF pos, double scale);
    void setTransitionType(QString newType);
    //const QMap < QString, QString > transitionParameters() const;
    void setTransitionParameters(const QDomElement&);
    void setTransitionDirection(bool inv);
    int transitionTrack() const;
    void setTransitionTrack(int track);
    QPixmap transitionPixmap() const;
    Transition *reparent(ClipItem * clip);
    bool isValid() const;
    GenTime transitionDuration() const;
    const ClipItem *referencedClip() const;

private:

    GenTime m_transitionStart;
    GenTime m_transitionDuration;
    QDomElement m_transitionParameters;

    /** The name of the transition to be displayed to user */
    QString m_transitionName;

    /** Should the transition be reversed */
    bool m_invertTransition;

    bool m_singleClip;

    /** The track to which the transition is attached*/
    int m_track;

    /** The clip to which the transition is attached */
    ClipItem *m_referenceClip;

    /** The 2nd clip to which the transition is attached */
    ClipItem *m_secondClip;
    int m_transitionTrack;

    /** Return the display name for a transition type */
    QString getTransitionName(const TRANSITIONTYPE & type);

    /** Return the transition type for a given name */
    TRANSITIONTYPE getTransitionForName(const QString & type);
};

#endif
