/***************************************************************************
                          aveffect.h  -  description
                             -------------------
    begin                : Wed Jan 8 2003
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

#ifndef AVEFFECT_H
#define AVEFFECT_H

#include <qstring.h>

/**Holds the full description of an effect or transition.
  *@author Jason Wood
  */

class AVEffect {
public: 
	AVEffect(const QString &name);
	~AVEffect();
private: // Private attributes
  /** The name of this effect. This is the internal name that get's sent to the renderer. */
  QString m_name;
  /** Caption is the name given to the user as this effect. It may differ from the name used internally. */
  QString m_caption;
  /** A description of this effect. */
  QString m_description;
};

#endif
