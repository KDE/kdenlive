/***************************************************************************
                          effectparameter  -  description
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
#ifndef EFFECTPARAMETER_H
#define EFFECTPARAMETER_H

#include <qstring.h>
/**
A parameter in an effect. Contains a list of keyframes that make up that parameter, and which conforms to the parameter description.

@author Jason Wood
*/
class EffectParameter{
public:
    EffectParameter(const QString &name);

    ~EffectParameter();
private:
	QString m_name;
};

#endif
