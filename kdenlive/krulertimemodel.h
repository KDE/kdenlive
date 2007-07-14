/***************************************************************************
                          krulertimemodel.h  -  description
                             -------------------
    begin                : Mon Jul 29 2002
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

#ifndef KRULERTIMEMODEL_H
#define KRULERTIMEMODEL_H

#include <krulermodel.h>

namespace Gui {

/**Implements a ruler which has HH:MM:SS:FF format
  *@author Jason Wood
  */

    class KRulerTimeModel:public KRulerModel {
      public:
	KRulerTimeModel();
	~KRulerTimeModel();
	static QString mapValueToText(int value, double frames);
  /** Returns a string representation of the value passed in the format HH:MM:SS:FF
  and assumes that the value passed is in frames.*/
	virtual QString mapValueToText(const int value) const;
  /** Finds a tick interval larger than or equal to the tick value passed which can be
    * considered a reasonable interval to display on screen. For example, when dealing
    * with hours:Minutes:Seconds, reasonable values might be every second, every 2
    * seconds, every 5 seconds, every 20 seconds, every minute, etc. If we are working
    * in millimeters, then every milimeter, every five millimeters, every cm, etc. This
    * function exists so that you can define your own scheme on how often ticks can
    * appear. This method should be able to handle values from 1 tick to MAXINT
    * intervals.
    */
	virtual int getTickDisplayInterval(const int tick) const;
  /** Sets the number of frames per second for this ruler model. */
	virtual void setNumFrames(double frames);
	virtual double numFrames() const;
      private:			// Private attributes
  /** holds the number of frames per second. Whilst we say frames, this could just as easily be milliseconds. */
	double m_numFrames;
    };

}				// namespace Gui
#endif
