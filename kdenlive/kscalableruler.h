/***************************************************************************
                          kscalableruler.h  -  description
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

#ifndef KSCALABLERULER_H
#define KSCALABLERULER_H

#include <kruler.h>

namespace Gui {

/**This ruler provides access to a ruler which can be scaled. Access is based upon
 the leftmost pixel currently displayed by the ruler. In essence, this class allows
  public access to a number of the proteced members of the KRuler class.
  *@author Jason Wood
  */

    class KScalableRuler:public KRuler {
      Q_OBJECT public:
	KScalableRuler(KRulerModel * model, QWidget * parent =
	    0, const char *name = 0);
	 KScalableRuler(QWidget * parent = 0, const char *name = 0);
	~KScalableRuler();
  /** Sets the range of the active part of the ruler. */
	void setRange(const int start, const int end);
	public slots:		// Public slots
  /** Sets the pixel which will be displayed leftmost or uppermost on the ruler. 0 pixels is equivalent to the value 0, other pixel amounts can be determined by multiplying by the scale factor. */
	void setStartPixel(int pixel);
  /** Sets the scale used to display values. The parameter specifies how many pixels should be used per value. */
	void setValueScale(double scale);
    };

}				// namespace Gui
#endif
