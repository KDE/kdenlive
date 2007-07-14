/***************************************************************************
                          krulermodel.cpp  -  description
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

#include "krulermodel.h"

namespace Gui {

    KRulerModel::KRulerModel() {
	setMinimumSmallTickSeperation(6);
	setMinimumLargeTickSeperation(30);
	setMinimumDisplayTickSeperation(100);
    } KRulerModel::~KRulerModel() {
    }

/** Returns a string representation of the value passed. The default model returns H:MM:SS and assumes that the value passed is in seconds. Extend this class to provide a different representation. */
    QString KRulerModel::mapValueToText(const int value) const {
	QString text = "";
	int hour, minute, second;

	 second = value;
	if (second < 0) {
	    second = -second;
	    text = "-";
	}

	hour = second / 3600;
	second %= 3600;
	minute = second / 60;
	second %= 60;

	text += QString::number(hour).rightJustify(1, '0', FALSE) + ":" +
	    QString::number(minute).rightJustify(2, '0', FALSE) + ":" +
	    QString::number(second).rightJustify(2, '0', FALSE);

	return text;
    }

/** returns the smallest number of pixels that must exist between two small ticks.
 */
    int KRulerModel::minimumSmallTickSeparation() const {
	return m_minimumSmallTickSeperation;
    }
/** Returns the minimum number of pixels that must exist between two large ticks. */
    int KRulerModel::minimumLargeTickSeparation() const {
	return m_minimumLargeTickSeperation;
    }
/** returns the minimum number of pixels which must exist between any two pieces of displayed text. */
    int KRulerModel::minimumTextSeparation() const {
	return m_minimumDisplayTickSeperation;
    } 

    int KRulerModel::getTickDisplayInterval(const int tick) const {
	int seconds = tick;

	if (seconds > 60) {
	    int minutes = seconds / 60;
	    if (minutes > 60) {
		int hours = minutes / 60;
		 minutes = hours * 60;
	    } else if (minutes > 30) {
		minutes = 60;
	    } else if (minutes > 20) {
		minutes = 30;
	    } else if (minutes > 10) {
		minutes = 20;
	    } else if (minutes > 5) {
		minutes = 10;
	    } else if (minutes > 2) {
		minutes = 5;
	    } else if (minutes > 1) {
		minutes = 2;
	    }
	    seconds = minutes * 60;
	} else if (seconds > 30) {
	    seconds = 60;
	} else if (seconds > 20) {
	    seconds = 30;
	} else if (seconds > 10) {
	    seconds = 20;
	} else if (seconds > 5) {
	    seconds = 10;
	} else if (seconds > 2) {
	    seconds = 5;
	} else if (seconds > 1) {
	    seconds = 2;
	} else {
	    seconds = 1;
	}

	return seconds;
    }

    void KRulerModel::setMinimumLargeTickSeperation(const int pixels) {
	m_minimumLargeTickSeperation = pixels;
    }

    void KRulerModel::setMinimumSmallTickSeperation(const int pixels) {
	m_minimumSmallTickSeperation = pixels;
    }

    void KRulerModel::setMinimumDisplayTickSeperation(const int pixels) {
	m_minimumDisplayTickSeperation = pixels;
    }

    void KRulerModel::setNumFrames(double frames)
    {
    }

    double KRulerModel::numFrames()
    {
    }

}				// namespace Gui
