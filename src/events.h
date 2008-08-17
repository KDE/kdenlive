/***************************************************************************
                          events.h  -  description
                             -------------------
    begin                : Feb 15 2008
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

#ifndef EVENTS_H
#define EVENTS_H

#include <QEvent>

#include "gentime.h"

class ProgressEvent : public QEvent {
public:
    ProgressEvent(int value, QEvent::Type eventType)
            : QEvent(eventType), m_val(value) {};
    int value() const {
        return m_val;
    };
private:
    int m_val;

};

#endif
