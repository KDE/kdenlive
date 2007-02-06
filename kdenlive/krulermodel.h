/***************************************************************************
                          krulermodel.h  -  description
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

#ifndef KRULERMODEL_H
#define KRULERMODEL_H

#include <qstring.h>

namespace Gui {

/**Contains the default model for a KRuler. Different models can be provided by extending this class.
  *@author Jason Wood
  */

    class KRulerModel {
      public:
	KRulerModel();
	virtual ~ KRulerModel();
  /** Returns a string representation of the value passed. The default model returns HH:MM:SS and assumes that the value passed is in seconds. Extend this class to provide a different representation. */
	virtual QString mapValueToText(const int value) const;
  /** Returns the minimum number of pixels that must exist between two large ticks. */
	int minimumLargeTickSeparation() const;
  /** returns the smallest number of pixels that must exist between two small ticks.
 */
	int minimumSmallTickSeparation() const;
  /** returns the minimum number of pixels which must exist between any two pieces of displayed text. */
	int minimumTextSeparation() const;
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

	  /** Sets the minimum number of pixels that must exist between any two large ticks. The ruler
   will pick the smallest tick count (as determined by getTickInterval) which leaves at least
    this number of pixels. */
	void setMinimumLargeTickSeperation(const int pixels);
    /** Sets the minimum number of pixels that must exist between any two small ticks. The ruler
   will pick the smallest tick count (as determined by getTickInterval) which leaves at least
    this number of pixels. */
	void setMinimumSmallTickSeperation(const int pixels);
    /** Sets the minimum number of pixels that must exist between any two text "ticks". The ruler
   will pick the smallest tick count (as determined by getTickInterval) which leaves at least
    this number of pixels. DisplayTicks are those which show text e.g. in HH:MM:SS format. */
	void setMinimumDisplayTickSeperation(const int pixels);
	virtual void setNumFrames(double frames);
      private:			// Private attributes
  /** Holds the minimum number of pixels that must exist between any two large ticks. */
	int m_minimumLargeTickSeperation;
  /** Holds the minimum number of pixels that must exist between any two large ticks. */
	int m_minimumSmallTickSeperation;
  /** Holds the minimum number of pixels that must exist between any two large ticks. */
	int m_minimumDisplayTickSeperation;
    };

}				// namespace Gui
#endif
