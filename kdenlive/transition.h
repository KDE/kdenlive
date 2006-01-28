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

#include <qstring.h>
#include <qptrlist.h>
#include <qdom.h>

#include "docclipref.h"



/**Describes a Transition, with a name, parameters keyframes, etc.
  *@author Jean-Baptiste Mardelle
  */

class Transition {
  public:
    Transition(const DocClipRef * clipa, const DocClipRef * clipb);

    ~Transition();

    public slots:double transitionStartTime();
    double transitionEndTime();
    uint transitionStartTrack();
    uint transitionEndTrack();
    Transition *clone();
    Transition *hasClip(const DocClipRef * clip);
  private:
    const DocClipRef *m_clipa;
    const DocClipRef *m_clipb;

};

#endif
