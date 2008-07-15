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
#include <QDomElement>
#include <QMap>
#include <QLinearGradient>

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

    Transition(const ItemInfo info, int transitiontrack, double scale, double fps, QDomElement params = QDomElement());
    virtual ~Transition();
    virtual void paint(QPainter *painter,
                       const QStyleOptionGraphicsItem *option,
                       QWidget *widget);
    virtual int type() const;
    /** Returns an XML representation of this transition. */
    QDomElement toXML();

    /** Return the track number of transition in the playlist*/
    int transitionEndTrack() const;
    Transition *clone();
    bool hasClip(const ClipItem * clip) const;
    bool belongsToClip(const ClipItem * clip) const;
    bool invertedTransition() const;
    QString transitionName() const;
    QString transitionTag() const;
    OPERATIONTYPE operationMode(QPointF pos, double scale);
    //const QMap < QString, QString > transitionParameters() const;
    void setTransitionParameters(const QDomElement params);
    void setTransitionDirection(bool inv);
    void setTransitionTrack(int track);
    QPixmap transitionPixmap() const;
    //Transition *reparent(ClipItem * clip);
    bool isValid() const;
    /** Transition should be linked to another track */
    void updateTransitionEndTrack(int newtrack);
    const ClipItem *referencedClip() const;
    Transition *clone(double scale);
private:
    bool m_singleClip;
    QLinearGradient m_gradient;
    QString m_name;
    /** contains the transition parameters */
    QDomElement m_parameters;
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
