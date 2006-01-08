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
#ifndef EFFECTPARAMDOUBLEDESC_H
#define EFFECTPARAMDOUBLEDESC_H

#include <effectparamdesc.h>

class QXmlAttributes;

/**
An effect parameter that holds a double value.

@author Jason Wood
*/
class EffectParamDoubleDesc : public EffectParamDesc
{
public:
    EffectParamDoubleDesc(const QXmlAttributes &attributes);

    ~EffectParamDoubleDesc();

	/** Creates a parameter that conforms to this parameter Description */
	virtual EffectKeyFrame *createKeyFrame(double time);
	virtual EffectKeyFrame *createKeyFrame(double time, double value);

	virtual Gui::KMMTrackPanel *createTrackPanel(Gui::KdenliveApp *app,
				Gui::KTimeLine *timeline,
				KdenliveDoc *document,
				DocTrackBase *docTrack,
				bool isCollapsed,
				QWidget *parent=0,
				const char *name=0);
	virtual Gui::KMMTrackPanel *createClipPanel(Gui::KdenliveApp *app,
				Gui::KTimeLine *timeline,
				KdenliveDoc *document,
				DocClipRef *clip,
				QWidget *parent=0,
				const char *name=0);

	virtual double max();
	virtual double min();

private:
	double m_min;
	double m_max;
};

#endif
