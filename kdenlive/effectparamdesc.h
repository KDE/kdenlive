/***************************************************************************
                         effectparamdesc  -  description
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
#ifndef EFFECTPARAMDESC_H
#define EFFECTPARAMDESC_H

#include <qstring.h>
#include <qstringlist.h>

class DocClipRef;
class DocTrackBase;
class EffectKeyFrame;

class KdenliveDoc;
class QWidget;
class QXmlAttributes;

namespace Gui {
    class KdenliveApp;
    class KTimeLine;
    class KMMTrackPanel;
}
/**
A description of an effect parameter

@author Jason Wood
*/ class EffectParamDesc
{
  public:
    EffectParamDesc(const QXmlAttributes & attributes);

     virtual ~ EffectParamDesc();

	/** Creates a parameter that conforms to this parameter Description */
    virtual EffectKeyFrame *createKeyFrame(double time) = 0;
    virtual EffectKeyFrame *createKeyFrame(double time, double value);
    virtual EffectKeyFrame *createKeyFrame(double time, QStringList parametersList);

	/** Creates a track panel that can edit this parameter type. */
    virtual Gui::KMMTrackPanel * createTrackPanel(Gui::KdenliveApp * app,
	Gui::KTimeLine * timeline,
	KdenliveDoc * document,
	DocTrackBase * docTrack,
	bool isCollapsed, QWidget * parent = 0, const char *name = 0) = 0;

	/** Creates a track panel that can edit this parameter type. */
    virtual Gui::KMMTrackPanel * createClipPanel(Gui::KdenliveApp * app,
	Gui::KTimeLine * timeline,
	KdenliveDoc * document,
	DocClipRef * clip, QWidget * parent = 0, const char *name = 0) = 0;

    virtual double max(uint ix = 0) const = 0;
    virtual double min(uint ix = 0) const = 0;

    virtual const QString endTag() const = 0;
    virtual const QString startTag() const = 0;
    virtual const QString list() const = 0;

    void setDescription(const QString & description);
    const QString & description() const;
    const QString & name() const {
	return m_name;
    } 
    
    const QString & type() const {
	return m_type;
    } 
    
    virtual const QString &value() const {
	return m_value;
    }

    const double &factor() const {
	return m_factor;
    }  
    
    virtual const QString &defaultValue() const {
	return m_default;
    } 
    
    virtual void setValue(const QString &value);

    virtual const QString complexParamName(uint ix) const;
    virtual const uint complexParamNum() const;

  private:
	/** The name of this parameter. */
     QString m_name;
	/** The type of this parameter. */
    QString m_type;
	/** The value of this parameter ( for constant parameters). */
    QString m_value;
	/** The default parameter value. */
    QString m_default;
	/** A human-readable description of what this parameter does within the effect. */
    QString m_description;
	/** The parameter value should be divided by this value before being passed to the filter. */
    double m_factor;
};

#endif
