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

/**A description of an effect. Specifies it's name, the parameters that it takes, the number and type of inputs, etc.
  *@author Jason Wood
  */

class EffectDesc {
public: 
	EffectDesc(const QString &name);
	~EffectDesc();
public: // Public attributes
  /** The name of this effect. */
  QString m_name;
};

#endif
