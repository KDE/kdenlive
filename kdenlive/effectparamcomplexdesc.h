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
#ifndef EFFECTPARAMCOMPLEXDESC_H
#define EFFECTPARAMCOMPLEXDESC_H

#include <qstringlist.h>

#include <effectparamdesc.h>

class QXmlAttributes;

/**
An effect parameter that holds a double value.

@author Jason Wood
*/
class EffectParamComplexDesc:public EffectParamDesc {
  public:
    EffectParamComplexDesc(const QXmlAttributes & attributes);

    ~EffectParamComplexDesc();

	/** Creates a parameter that conforms to this parameter Description */
    virtual EffectKeyFrame *createKeyFrame(double time);
    virtual EffectKeyFrame *createKeyFrame(double time,
	QStringList parametersList);

    virtual Gui::KMMTrackPanel * createTrackPanel(Gui::KdenliveApp * app,
	Gui::KTimeLine * timeline,
	KdenliveDoc * document,
	DocTrackBase * docTrack,
	bool isCollapsed, QWidget * parent = 0, const char *name = 0);
    virtual Gui::KMMTrackPanel * createClipPanel(Gui::KdenliveApp * app,
	Gui::KTimeLine * timeline,
	KdenliveDoc * document,
	DocClipRef * clip, QWidget * parent = 0, const char *name = 0);

    virtual const QString endTag() const;
    virtual const QString startTag() const;

    virtual double max(uint ix = 0) const;
    virtual double min(uint ix = 0) const;
    virtual const double &defaultValue(uint ix = 0) const;

    const QString complexParamName(uint ix) const;
    const uint complexParamNum() const;

  private:
     QStringList m_mins;
    QStringList m_names;
    QStringList m_maxs;
    QStringList m_defaults;
    QString m_starttag;
    QString m_endtag;
};

#endif
