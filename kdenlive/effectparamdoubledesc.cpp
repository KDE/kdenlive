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

EffectParamDoubleDesc::EffectParamDoubleDesc(const QXmlAttributes &attributes)
 					: EffectParamDesc(attributes)
{
	m_min = attributes.value("min").toDouble();
	m_max = attributes.value("max").toDouble();
}

EffectParamDoubleDesc::~EffectParamDoubleDesc()
{
}

//Virtual
double EffectParamDoubleDesc::max()
{
	return m_max;
}

//Virtual
double EffectParamDoubleDesc::min()
{
	return m_min;
}

// virtual
EffectKeyFrame *EffectParamDoubleDesc::createKeyFrame(double time)
{
	return new EffectDoubleKeyFrame(time, m_max);
}

// virtual
EffectKeyFrame *EffectParamDoubleDesc::createKeyFrame(double time, double value)
{
	if (value>m_max) value = m_max;
	if (value<m_min) value = m_min;
	return new EffectDoubleKeyFrame(time, value);
}

// virtual
Gui::KMMTrackPanel *EffectParamDoubleDesc::createTrackPanel(Gui::KdenliveApp *app,
				Gui::KTimeLine *timeline,
				KdenliveDoc *document,
				DocTrackBase *docTrack,
				bool isCollapsed,
				QWidget *parent,
				const char *name)
{
	kdWarning() << "EffectParamDoubleDesc::createTrackPanel()" << endl;
	#warning - need to pass in the effect name/index from somewhere.
	return new Gui::KMMTrackKeyFramePanel( timeline, document, docTrack, false, "alphablend", 0,  name, Gui::EFFECTKEYFRAMETRACK, parent, name);
}

// virtual
Gui::KMMTrackPanel *EffectParamDoubleDesc::createClipPanel(Gui::KdenliveApp *app,
				Gui::KTimeLine *timeline,
				KdenliveDoc *document,
				DocClipRef *clip,
				QWidget *parent,
				const char *name)
{
	kdWarning() << "EffectParamDoubleDesc::createTrackPanel()" << endl;
	#warning - need to pass in the effect name/index from somewhere.
	return new Gui::KMMClipKeyFramePanel( timeline, document, clip, EffectParamDesc::name(), 0, name, parent, name);
}
