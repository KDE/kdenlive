/***************************************************************************
                          krulertimemodel.cpp  -  description
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

#include <kdebug.h>
#include <math.h>

#include "krulertimemodel.h"
#include "kdenlivesettings.h"
#include "timecode.h"


namespace Gui {

    KRulerTimeModel::KRulerTimeModel():KRulerModel() {
	setNumFrames(KdenliveSettings::defaultfps());
	setMinimumSmallTickSeperation(15);
	setMinimumLargeTickSeperation(50);
	setMinimumDisplayTickSeperation(100);
    } KRulerTimeModel::~KRulerTimeModel() {
    }

    QString KRulerTimeModel::mapValueToText(int value, double frames) {
	Timecode t;
	if (frames == 30000.0 / 1001.0 ) t.setFormat(30, true);
    	else t.setFormat(frames);
	return t.getTimecodeFromFrames(value);
    }


    QString KRulerTimeModel::mapValueToText(const int value) const {
	return KRulerTimeModel::mapValueToText(value, m_numFrames);
    } 

    int KRulerTimeModel::getTickDisplayInterval(const int tick) const {
	int seconds = tick;

	if (seconds > 3600 * numFrames()) {
    	    int hour = (tick / (3600 * (int) numFrames())) + 1;
    	    seconds = hour * 3600 * (int) numFrames();
  	} else if (seconds > 60 * (int) numFrames()) {
    	    int minute = (tick / (60 * (int) numFrames())) + 1;
	    if (minute > 30) {
		minute = 60;
	    } else if (minute > 20) {
		minute = 30;
	    } else if (minute > 15) {
		minute = 20;
	    } else if (minute > 10) {
		minute = 15;
	    } else if (minute > 5) {
		minute = 10;
	    } else if (minute > 1) {
		minute = 5;
	    }
	    seconds = minute * 60 * (int) numFrames();
  	} else if (seconds >= (int) numFrames()) {
    	    seconds /= (int) numFrames();
	    seconds++;

	    if (seconds > 30) {
		seconds = 60;
	    } else if (seconds > 20) {
		seconds = 30;
	    } else if (seconds > 15) {
		seconds = 20;
	    } else if (seconds > 10) {
		seconds = 15;
	    } else if (seconds > 5) {
		seconds = 10;
	    } else if (seconds > 2) {
		seconds = 5;
	    }

	    seconds *= (int) numFrames();
	} else {
	    int count;
	    for (count = 1; count < numFrames(); count++) {
		#warning - will not calculate correct intervals for none-integer frame rates.
		if ((int) numFrames() % count != 0)
		    continue;
		if (count >= seconds)
		    break;
	    }

	    seconds = count;
	}

	return seconds;
    }

/** Sets the number of frames per second for this ruler model. */
    // virtual
    void KRulerTimeModel::setNumFrames(double frames) {
	m_numFrames = frames; //floor(frames + 0.5);
    }

    // virtual
    double KRulerTimeModel::numFrames() const {
	return m_numFrames;
    }

}				// namespace Gui
