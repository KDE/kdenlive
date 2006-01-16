/***************************************************************************
                          kfixedruler.h  -  description
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

#ifndef KFIXEDRULER_H
#define KFIXEDRULER_H

#include <kruler.h>

namespace Gui {

/**This class provides a simplified version of KRuler. Instead of the "start position and zoom" functionality, this class provides "start and end values",
	allowing you to set up a non-panning ruler. Examples of uses for this class could be a media player, where the position indicator ticks from the
	start to the end.
  *@author Jason Wood
  */

    class KFixedRuler:public KRuler {
      Q_OBJECT public:
	KFixedRuler(int startValue, int endValue, int margin =
	    20, KRulerModel * model = 0, QWidget * parent =
	    0, const char *name = 0);
	 KFixedRuler(KRulerModel * model, QWidget * parent =
	    0, const char *name = 0);
	 KFixedRuler(QWidget * parent = 0, const char *name = 0);
	~KFixedRuler();
  /** Sets the range of values which are displayed on the timeline. */
	void setRange(const int min, const int max);
  /** Sets the end value of the ruler - that is the last value which should be displayed on the ruler. */
	void setMaxValue(const int end);
  /** Sets the start value for the ruler - that is, the leftmost value that should be displayed. */
	void setMinValue(const int start);
  /** Returns the maximum value of this ruler. */
	int maxValue() const;
  /** Returns the minimum value of this ruler */
	int minValue() const;
  /** Returns the current margin of the ruler */
	int margin() const;
  /** Sets the margin on either side of the ruler. This determines how much "out of range" ruler is displayed on screen. Useful for tidying up the ruler. */
	void setMargin(const int margin);
	private slots:		// Private slots
  /** Calculates the required values to ensure that startValue is at the start of the ruler, and endValue is at the end of it. */
	void calculateViewport();
	 signals:		// Signals
  /** endValueChanged(int) is emitted when the end value (the rightmost on the ruler) has been changed. The new end value is emitted. */
	void endValueChanged(int);
  /** startValueChanged(int) is emitted when the start value (the leftmost on the ruler) has changed. The new start value is emitted. */
	void startValueChanged(int);
      private:			// Private attributes
  /** How much margin exists on either side of the ruler, in pixels */
	int m_margin;
    };

}				// namespace Gui
#endif
