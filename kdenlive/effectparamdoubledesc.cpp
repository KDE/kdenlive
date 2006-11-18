/***************************************************************************
                          effectparamdoubledesc  -  description
                             -------------------
    begin                : Fri Jan 2 2004
    copyright            : (C) 2004 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "effectparamdoubledesc.h"

#include <qxml.h>

#include <kdebug.h>

#include "effectdoublekeyframe.h"

#include "kmmclipkeyframepanel.h"
#include "kmmtrackkeyframepanel.h"

EffectParamDoubleDesc::
EffectParamDoubleDesc(const QXmlAttributes & attributes)
:  EffectParamDesc(attributes)
{
    m_min = attributes.value("min").toDouble();
    m_max = attributes.value("max").toDouble();
    m_starttag = attributes.value("starttag");
    m_endtag = attributes.value("endtag");
    m_factor = attributes.value("factor").toDouble();
    if (m_factor == 0.0) m_factor = 1.0;
    m_list = attributes.value("paramlist");

    if (m_starttag == "")
	m_starttag = "start";
    if (m_endtag == "")
	m_endtag = "end";
}

EffectParamDoubleDesc::~EffectParamDoubleDesc()
{
}

//Virtual
void EffectParamDoubleDesc::setMax(int max)
{
    m_max = max;
}

//Virtual
const QString EffectParamDoubleDesc::list() const
{
    return m_list;
}

//Virtual
double EffectParamDoubleDesc::max(uint) const
{
    return m_max;
}

//Virtual
double EffectParamDoubleDesc::min(uint) const
{
    return m_min;
}

//Virtual
const QString EffectParamDoubleDesc::startTag() const
{
    return m_starttag;
}

//Virtual
const QString EffectParamDoubleDesc::endTag() const
{
    return m_endtag;
}

// virtual
EffectKeyFrame *EffectParamDoubleDesc::createKeyFrame(double time)
{
    return new EffectDoubleKeyFrame(time, defaultValue().toInt());
}

// virtual
EffectKeyFrame *EffectParamDoubleDesc::createKeyFrame(double time,
    double value)
{
    if (value > m_max)
	value = m_max;
    if (value < m_min)
	value = m_min;
    return new EffectDoubleKeyFrame(time, value);
}

// virtual
Gui::KMMTrackPanel *
    EffectParamDoubleDesc::createTrackPanel(Gui::KdenliveApp *,
    Gui::KTimeLine * timeline, KdenliveDoc * document,
    DocTrackBase * docTrack, bool isCollapsed, QWidget * parent,
    const char *name)
{
    kdWarning() << "EffectParamDoubleDesc::createTrackPanel()" << endl;
#warning - need to pass in the effect name/index from somewhere.
    return new Gui::KMMTrackKeyFramePanel(timeline, document, docTrack,
	false, Gui::EFFECTKEYFRAMETRACK, parent,
	name);
}

// virtual
Gui::KMMTrackPanel *
    EffectParamDoubleDesc::createClipPanel(Gui::KdenliveApp *,
    Gui::KTimeLine * timeline, KdenliveDoc * document, DocClipRef * clip,
    QWidget * parent, const char *name)
{
    kdWarning() << "EffectParamDoubleDesc::createTrackPanel()" << endl;
#warning - need to pass in the effect name/index from somewhere.
    return new Gui::KMMClipKeyFramePanel(timeline, document, clip,
	EffectParamDesc::name(), 0, name, parent, name);
}
