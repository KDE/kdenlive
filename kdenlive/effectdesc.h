/***************************************************************************
                         effectdesc.h  -  description
                            -------------------
   begin                : Sun Feb 9 2003
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

#ifndef EFFECTDESC_H
#define EFFECTDESC_H

#include <qstring.h>
#include <qptrlist.h>

/**A description of an effect. Specifies it's name, the parameters that it takes, the number and type of inputs, etc.
  *@author Jason Wood
  */

class Effect;
class EffectParamDesc;

class EffectDesc
{
public:
	EffectDesc( const QString &name );
	~EffectDesc();
	/** Returns the name of this effect. */
	const QString &name() const ;
	/** Adds an input to this description. An input might be a video stream, and audio stream, or it may require both. */
	void addInput( const QString &name, bool video, bool audio );
	/** Adds an new parameter to this description. Examples of parameters could be opacity level, text, colour, font, etc. */
	void addParameter( EffectParamDesc *param );
	uint numParameters() const;
	EffectParamDesc *parameter(uint index);

	/** Creates an effect with the correct number of parameters for this effect description. Use the given preset if one exists.*/
	Effect *createEffect(const QString &preset = QString::null);
private:
	/** The name of this effect. */
	QString m_name;

	QPtrList<EffectParamDesc> m_params;
	QPtrList<Effect> m_presets;

	/** find and return the preset effect with the given name. */
	Effect *findPreset(const QString &name);
};

#endif
