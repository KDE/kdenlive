/***************************************************************************
                          effectparamcomplexdesc  -  description
                             -------------------
    begin                : Fri Jan 16 2006
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
#include "effectparamcomplexdesc.h"

#include <qxml.h>

#include <kdebug.h>

#include "effectcomplexkeyframe.h"

#include "kmmclipkeyframepanel.h"
#include "kmmtrackkeyframepanel.h"

EffectParamComplexDesc::
EffectParamComplexDesc(const QXmlAttributes & attributes)
:  EffectParamDesc(attributes)
{
    m_mins = QStringList::split(";", attributes.value("min"));
    m_maxs = QStringList::split(";", attributes.value("max"));
    m_names = QStringList::split(";", attributes.value("name"));
    m_starttag = attributes.value("starttag");
    m_endtag = attributes.value("endtag");
    if (m_starttag == "")
	m_starttag = "start";
    if (m_endtag == "")
	m_endtag = "end";
    m_defaults = QStringList::split(";", attributes.value("default"));
}

EffectParamComplexDesc::~EffectParamComplexDesc()
{
}

const QString EffectParamComplexDesc::complexParamName(uint ix) const
{
    return m_names[ix];
}

const uint EffectParamComplexDesc::complexParamNum() const
{
    return m_names.size();
}

//Virtual
double EffectParamComplexDesc::max(uint ix) const
{
    return m_maxs[ix].toDouble();
}

//Virtual
double EffectParamComplexDesc::min(uint ix) const
{
    return m_mins[ix].toDouble();
}


//Virtual
const double &EffectParamComplexDesc::defaultValue(uint ix) const
{
    return m_defaults[ix].toDouble();
}

//Virtual
const QString EffectParamComplexDesc::startTag() const
{
    return m_starttag;
}

//Virtual
const QString EffectParamComplexDesc::endTag() const
{
    return m_endtag;
}

//Virtual
const QString EffectParamComplexDesc::list() const
{
    return QString();
}

// virtual
EffectKeyFrame *EffectParamComplexDesc::createKeyFrame(double time)
{
    // Internally, the keyframe values are in a range from 0 to 100.
    return new EffectComplexKeyFrame(time, m_defaults);
}

// virtual
EffectKeyFrame *EffectParamComplexDesc::createKeyFrame(double time,
    QStringList parametersList)
{
    return new EffectComplexKeyFrame(time, parametersList);
}

// virtual
Gui::KMMTrackPanel *
    EffectParamComplexDesc::createTrackPanel(Gui::KdenliveApp *,
    Gui::KTimeLine * timeline, KdenliveDoc * document,
    DocTrackBase * docTrack, bool isCollapsed, QWidget * parent,
    const char *name)
{
    kdWarning() << "EffectParamComplexDesc::createTrackPanel()" << endl;
#warning - need to pass in the effect name/index from somewhere.
    return new Gui::KMMTrackKeyFramePanel(timeline, document, docTrack,
	false, "alphablend", 0, name, Gui::EFFECTKEYFRAMETRACK, parent,
	name);
}

// virtual
Gui::KMMTrackPanel *
    EffectParamComplexDesc::createClipPanel(Gui::KdenliveApp *,
    Gui::KTimeLine * timeline, KdenliveDoc * document, DocClipRef * clip,
    QWidget * parent, const char *name)
{
    kdWarning() << "EffectParamComplexDesc::createTrackPanel()" << endl;
#warning - need to pass in the effect name/index from somewhere.
    return new Gui::KMMClipKeyFramePanel(timeline, document, clip,
	EffectParamDesc::name(), 0, name, parent, name);
}
