/***************************************************************************
                          transition.cpp  -  description
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

#include "transition.h"
#include "docclipref.h"
#include "kdenlivesettings.h"

#include <kdebug.h>
#include <kiconloader.h>
#include <klocale.h>
#include <qdom.h>

/* Transitions can be either be
    1) placed on a track. They are not dependant of any clip // not implemented yet
    2) Attached to one clip. They move with the clip
    3) Attached to two clips. They automatically adjust to the length of the overlapping area //disabled for the moment
*/

/* create an "automatic duration" transition */
Transition::Transition(const DocClipRef * clipa, const DocClipRef * clipb)
{
    m_invertTransition = false;
    m_singleClip = true;
    m_secondClip = NULL;
    m_transitionType = "luma";
    m_transitionName = i18n("Crossfade");
    m_transitionWipeDirection = "right";

    if (clipb) {
        // Transition is an automatic transition between 2 clips
        m_singleClip = true;
        if (clipa->trackNum()>clipb->trackNum()) {
            m_referenceClip = clipb;
            m_secondClip = clipa;
        }
        else {
            m_referenceClip = clipa;
            m_secondClip = clipb;
        }
    
        if (m_referenceClip->trackStart() < m_secondClip->trackStart())
		m_invertTransition = true;

	GenTime startb = m_secondClip->trackStart();
        GenTime starta = m_referenceClip->trackStart();
	GenTime endb = m_secondClip->trackEnd();
        GenTime enda = m_referenceClip->trackEnd();
	GenTime transitionDuration;
	if (starta > startb && starta < endb) {
		m_transitionStart = GenTime(0.0);
        	if (endb < enda) m_transitionDuration = endb - starta;
		else m_transitionDuration = enda - starta;
	}
	else if (startb > starta && startb < enda){
		m_transitionStart = startb - starta;
		if (endb > enda) m_transitionDuration = enda - startb;
		else m_transitionDuration = endb - startb;
	}
	else {
		if (starta > endb)
			m_transitionStart = GenTime(0.0);
		else m_transitionStart = enda - starta -GenTime(3.0);

		m_transitionDuration = GenTime(3.0);
	}

	if (m_transitionDuration < GenTime(0.12)) m_transitionDuration = GenTime(0.12);
	m_secondClip = 0; // disable auto transition for the moment
    }
    else {
        // Transition is attached to a single clip
        m_referenceClip = clipa;
        m_transitionStart = GenTime(0.0);
        
        // Default duration = 1,5 seconds
        if (GenTime(1.5) > m_referenceClip->cropDuration()) 
            m_transitionDuration = m_referenceClip->cropDuration();
        else m_transitionDuration = GenTime(1.5);
        m_secondClip = 0;
    }
}

/* create an "simple" transition (type 2) */
Transition::Transition(const DocClipRef * clipa)
{
    m_invertTransition = false;
    m_singleClip = true;
    m_secondClip = NULL;
    m_transitionType = "luma";
    m_transitionName = i18n("Crossfade");
    m_transitionWipeDirection = "right";

        m_referenceClip = clipa;
        m_transitionStart = GenTime(0.0);
        
        // Default duration = 2.5 seconds
        if (GenTime(1.5) > m_referenceClip->cropDuration()) 
            m_transitionDuration = m_referenceClip->cropDuration();
        else m_transitionDuration = GenTime(2.5);
        m_secondClip = 0;
}

/* create an "simple" transition (type 2) around a given time*/
Transition::Transition(const DocClipRef * clipa, const GenTime &time)
{
    m_invertTransition = false;
    m_singleClip = true;
    m_secondClip = NULL;
    m_transitionType = "luma";
    m_transitionName = i18n("Crossfade");
    m_transitionWipeDirection = "right";
    
    // Default duration = 2.5 seconds
    GenTime defaultTransitionDuration = GenTime(2.5);

    m_referenceClip = clipa;
    if (time - m_referenceClip->trackStart() < GenTime(2.0)) m_transitionStart = GenTime(0.0);
    else if (m_referenceClip->trackEnd() - time < GenTime(2.0)) m_transitionStart = m_referenceClip->cropDuration() - defaultTransitionDuration;
    else m_transitionStart = time - m_referenceClip->trackStart() - GenTime(1.0);
    
    if (m_transitionStart + defaultTransitionDuration > m_referenceClip->cropDuration()) 
        m_transitionDuration = m_referenceClip->cropDuration() - m_transitionStart;
    else m_transitionDuration = defaultTransitionDuration;
    
    if (time > clipa->trackMiddleTime()) m_invertTransition = true;
    m_secondClip = 0;
}

/* create an "simple" transition (type 2) */
Transition::Transition(const DocClipRef * clipa, const QString & type, const GenTime &startTime, const GenTime &endTime, bool inverted)
{
    m_invertTransition = inverted;
    m_singleClip = true;
    m_secondClip = NULL;
    m_transitionType = type;
    if (m_transitionType == "composite") m_transitionName = i18n("Wipe");
    else if (m_transitionType == "pip")	m_transitionName = i18n("Pip");
    else m_transitionName = i18n("Crossfade");
    m_transitionWipeDirection = "right";


    GenTime duration = endTime - startTime;
    
    // Default duration = 2.5 seconds
    GenTime defaultTransitionDuration = GenTime(2.5);
    
    m_referenceClip = clipa;
    if (startTime < m_referenceClip->trackStart()) m_transitionStart = GenTime(0.0);
    else if (startTime > m_referenceClip->trackEnd()) m_transitionStart = m_referenceClip->cropDuration() - defaultTransitionDuration;
    else m_transitionStart = startTime - m_referenceClip->trackStart();
    
    if (m_transitionStart + duration > m_referenceClip->cropDuration()) 
        m_transitionDuration = m_referenceClip->cropDuration() - m_transitionStart;
    else m_transitionDuration = duration;
    m_secondClip = 0;
}

// create a transition from XML
Transition::Transition(const DocClipRef * clip, QDomElement transitionElement)
{
	//QDomNode node = transitionXml.firstChild();
	//QDomElement transitionElement = transitionXml.toElement(); //node.toElement();

	//kdDebug()<<"+++++"<<transitionXml.toString()<<endl;


	m_referenceClip = clip;
	m_singleClip = true;
        m_secondClip = NULL;
	m_transitionStart = GenTime (transitionElement.attribute("start", QString::null).toInt(),KdenliveSettings::defaultfps());
	m_transitionDuration = GenTime(transitionElement.attribute("end", QString::null).toInt(),KdenliveSettings::defaultfps()) - m_transitionStart;

	m_transitionStart = m_transitionStart - clip->trackStart();

        m_invertTransition = transitionElement.attribute("inverted", "0").toInt();
	m_transitionType = transitionElement.attribute("type", QString::null);

	// load transition parameters
        typedef QMap<QString, QString> ParamMap;
        ParamMap params;
        for( QDomNode n = transitionElement.firstChild(); !n.isNull(); n = n.nextSibling() )
        {
            QDomElement paramElement = n.toElement();
            params[paramElement.tagName()] = paramElement.attribute("value", QString::null);
        }
        if (!params.isEmpty()) setTransitionParameters(params);
}

Transition::~Transition()
{
}

void Transition::setTransitionType(QString newType)
{
    m_transitionType = newType;
    if (m_transitionType == "composite") m_transitionName = i18n("Wipe");
    else if (m_transitionType == "pip")	m_transitionName = i18n("Pip");
    else m_transitionName = i18n("Crossfade");
}

QString Transition::transitionType()
{
    return m_transitionType;
}

QString Transition::transitionName()
{
    return m_transitionName;
}

void Transition::setTransitionParameters(const QMap < QString, QString > parameters)
{
    m_transitionParameters = parameters;
}

const QMap < QString, QString > Transition::transitionParameters()
{
    return m_transitionParameters;
}

bool Transition::invertTransition()
{
    if (!m_singleClip) {
        if (m_referenceClip->trackStart() < m_secondClip->trackStart()) return true;
        else return false;
    }
    return m_invertTransition;
}

void Transition::setTransitionWipeDirection(QString direction)
{
    m_transitionWipeDirection = direction;
}

QPixmap Transition::transitionPixmap()
{
    if (m_transitionType == "luma") {
	if (invertTransition()) return KGlobal::iconLoader()->loadIcon("kdenlive_trans_down", KIcon::Small, 15);
	else return KGlobal::iconLoader()->loadIcon("kdenlive_trans_up", KIcon::Small, 15);
    }
    else if (m_transitionType == "composite") {
        if (m_transitionWipeDirection == "right") return KGlobal::iconLoader()->loadIcon("kdenlive_trans_wiper", KIcon::Small, 15);
	else if (m_transitionWipeDirection == "left") return KGlobal::iconLoader()->loadIcon("kdenlive_trans_wipel", KIcon::Small, 15);
	else if (m_transitionWipeDirection == "down") return KGlobal::iconLoader()->loadIcon("kdenlive_trans_wiped", KIcon::Small, 15);
	else return KGlobal::iconLoader()->loadIcon("kdenlive_trans_wipeu", KIcon::Small, 15);
    }
    else return KGlobal::iconLoader()->loadIcon("kdenlive_trans_pip", KIcon::Small, 15);
}

void Transition::setTransitionDirection(bool inv)
{
    m_invertTransition = inv;
}


int Transition::transitionStartTrack()
{
    return m_referenceClip->playlistTrackNum();
}

int Transition::transitionEndTrack()
{
    if (!m_singleClip) return m_secondClip->playlistTrackNum();
    // #Warning: Should point to the previous video track. Currently we substract by 2 
    // because we don't want the audio track, but should find a better way to get the track number.
    //else if (transitionStartTrack()>1) return m_referenceClip->trackNum()-2; 
    //else 
    return transitionStartTrack()-1;
}

GenTime Transition::transitionStartTime()
{
    if (!m_singleClip) {
        GenTime startb = m_secondClip->trackStart();
        GenTime starta = m_referenceClip->trackStart();
        if (startb > m_referenceClip->trackEnd()) return m_referenceClip->trackEnd() - GenTime(0.12);
        if (startb > starta)
	   return startb;
        return starta;
    }
    else return m_referenceClip->trackStart() + m_transitionStart;
}


GenTime Transition::transitionEndTime()
{
    if (!m_singleClip) {
        GenTime endb = m_secondClip->trackEnd();
        GenTime enda = m_referenceClip->trackEnd();
        if (m_secondClip->trackStart() > enda) return enda;
        if (endb < m_referenceClip->trackStart()) return m_referenceClip->trackStart() + GenTime(0.12);
        else if (endb > enda) return enda;
        else return endb;
    }
    else { 
        if (m_transitionStart + m_transitionDuration > m_referenceClip->cropDuration())
            return m_referenceClip->trackEnd();
        return m_referenceClip->trackStart() + m_transitionStart + m_transitionDuration;
    }
}


void Transition::resizeTransitionStart(GenTime time)
{
    if (!m_singleClip) return; //cannot resize automatic transitions
    if (time < m_referenceClip->trackStart()) time = m_referenceClip->trackStart();
    // Transitions shouldn't be shorter than 3 frames, about 0.12 seconds
    if ( transitionEndTime().ms() - time.ms() < 120.0) time = transitionEndTime() - GenTime(0.12);
    m_transitionDuration =m_transitionDuration - (time - m_referenceClip->trackStart() - m_transitionStart);
    m_transitionStart = time - m_referenceClip->trackStart();
}

void Transition::resizeTransitionEnd(GenTime time)
{
    if (!m_singleClip) return; //cannot resize automatic transitions
    if (time > m_referenceClip->trackEnd()) time = m_referenceClip->trackEnd();
    // Transitions shouldn't be shorter than 3 frames, about 0.12 seconds
    if ( time.ms() - transitionStartTime().ms() < 120.0) time = transitionStartTime() + GenTime(0.12);
    m_transitionDuration = time - ( m_referenceClip->trackStart() + m_transitionStart);
}

void Transition::moveTransition(GenTime time)
{
    if (!m_singleClip) return; //cannot move automatic transitions
    if (m_transitionStart + time < GenTime(0.0)) m_transitionStart = GenTime(0.0);
    else if ( m_transitionStart + time > m_referenceClip->cropDuration() - m_transitionDuration)
        m_transitionStart = m_referenceClip->cropDuration() - m_transitionDuration;
    else m_transitionStart = m_transitionStart + time;
}

bool Transition::hasClip(const DocClipRef * clip)
{
    if (clip == m_secondClip) return true;
    return false;
}

Transition *Transition::clone()
{
    if (m_singleClip || m_secondClip == 0)
        return new Transition::Transition(m_referenceClip);
    else
        return new Transition::Transition(m_referenceClip, m_secondClip);
}

QDomElement Transition::toXML()
{
    QDomDocument doc;
    QDomElement effect = doc.createElement("transition");
    effect.setAttribute("type", transitionType());
    effect.setAttribute("inverted", invertTransition());
    effect.setAttribute("start", transitionStartTime().frames(KdenliveSettings::defaultfps()));
    effect.setAttribute("end", transitionEndTime().frames(KdenliveSettings::defaultfps()));
    if (m_secondClip) {
        effect.setAttribute("clipb_starttime", m_secondClip->trackStart().frames(KdenliveSettings::defaultfps()));
        effect.setAttribute("clipb_track", transitionEndTrack());
    }
    

    QMap<QString, QString>::Iterator it;
    for ( it = m_transitionParameters.begin(); it != m_transitionParameters.end(); ++it ) {
        QDomElement param = doc.createElement(it.key());
        param.setAttribute("value",it.data());
        effect.appendChild(param);
    }

    return effect;
}

