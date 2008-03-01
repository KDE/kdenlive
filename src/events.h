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
	ProgressEvent( int value, QEvent::Type eventType  )
		: QEvent( eventType ), m_val( value ) {};
	int value() const { return m_val; };
private:
	int m_val;
	
};


class EffectEvent : public QEvent {
public:
	EffectEvent( GenTime pos, int track, QDomElement xml, QEvent::Type eventType  )
		: QEvent( eventType ), m_pos( pos ), m_track(track), m_xml(xml) {
	if (xml.isNull()) kDebug()<<"--- ERROR, TRYING TO APPEND NULL EFFECT EVENT";	
	if (m_xml.isNull()) kDebug()<<"--- ERROR, TRYING TO APPEND NULL EFFECT EVENT 2";
	};
	GenTime pos() const { return m_pos; };
	int track() const { return m_track; };
	QDomElement xml() const { return m_xml; };
private:
	GenTime m_pos;
	int m_track;
	QDomElement m_xml;
	
};

#endif
