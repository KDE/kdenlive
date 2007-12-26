/***************************************************************************
                          effect.h  -  description
                             -------------------
    begin                : Sun Feb 16 2003
    copyright            : (C) 2003 by Jason Wood
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

#ifndef EFFECT_H
#define EFFECT_H

#include <qstring.h>
#include <qstringlist.h>
#include <qptrlist.h>
#include <qdom.h>
#include <qmap.h>

#include "effectdesc.h"

class DocClipRef;
class EffectDesc;
class EffectParameter;

/**Describes an effect, with a name, parameters keyframes, etc.
  *@author Jason Wood
  */

class Effect {
  public:
    Effect(const EffectDesc & desc, const QString & id, const QString & id = NULL);

    ~Effect();

	/** Returns an XML representation of this effect. */
    QDomDocument toXML();
	/** Returns a full XML representation of this effect for file saving. */
    QDomDocument toFullXML(const QString &effectName);

    const QString & name() const {
	return m_desc.name();
    }

    const QString & group() const {
	return m_group;
    }

    void addParameter(const QString & name);
    void setEnabled(bool isOn);
    bool isEnabled();

	/** Produce a clone of this effect. */
    Effect *clone();

	/** Creates an effect from the specified xml */
    static Effect *createEffect(const EffectDesc & desc,
	const QDomElement & effect);

    const EffectDesc & effectDescription() const {
	return m_desc;
    } 

   EffectParameter *parameter(const uint ix);
   QMap <QString, QString> getParameters(DocClipRef *clip);

	/** Creates a new keyframe at specified time and returns the new key's index */
    uint addKeyFrame(const uint ix, double time);
    uint addKeyFrame(const uint ix, double time, double value);
    void addKeyFrame(const uint ix, double time, QStringList values);
    void setTempFile(QString tmpFile);
    void setGroup(const QString &group);
    uint addInitialKeyFrames(int ix);

    QString tempFileName() {
	return m_paramFile;
    }

  private:
    const EffectDesc & m_desc;
    QString m_id;
    QString m_group;
    QString m_paramFile;
    QPtrList < EffectParameter > m_paramList;
    bool m_enabled;
};

#endif
