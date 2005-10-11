/***************************************************************************
                          kscalableruler.cpp  -  description
                             -------------------
    begin                : Sun Jul 7 2002
    copyright            : (C) 2002 by Jason Wood
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

#include "kscalableruler.h"

namespace Gui
{

KScalableRuler::KScalableRuler(KRulerModel *model, QWidget *parent, const char *name ) :
					 KRuler(model, parent, name)
{
}

KScalableRuler::KScalableRuler(QWidget *parent, const char *name ) :
						KRuler(0, parent, name)
{
}

KScalableRuler::~KScalableRuler()
{
}

/** Sets the pixel which will be displayed leftmost or uppermost on the ruler. 0 pixels is equivalent to the value 0, other pixel amounts can be determined by multiplying by the scale factor. */
void KScalableRuler::setStartPixel(int pixel)
{
	KRuler::setStartPixel(pixel);
}

/** Sets the scale used to display values. The parameter specifies how many pixels should be used per value. */
void KScalableRuler::setValueScale(double scale){
	KRuler::setValueScale(scale);
}

/** Sets the range of the active part of the ruler. */
void KScalableRuler::setRange(const int start, const int end)
{
	KRuler::setRange(start, end);
}

} // namespace Gui
