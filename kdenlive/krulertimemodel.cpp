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

#include "krulertimemodel.h"

KRulerTimeModel::KRulerTimeModel()
{
	setNumFrames(25);
	setMinimumSmallTickSeperation(10);
	setMinimumLargeTickSeperation(40);
	setMinimumDisplayTickSeperation(100);	
}

KRulerTimeModel::~KRulerTimeModel()
{
}

QString KRulerTimeModel::mapValueToText(const int value) const
{
	QString text = "";
	int frame;

	if(value >=0) {
		frame = value;
	} else {
		frame = -value;
		text = "-";
	}			
  
	int second = frame / numFrames();
	frame %= numFrames();

	int minute = second/60;
	second %= 60;

	int hour = minute/60;
	minute %= 60;

	text += QString::number(hour).rightJustify(1, '0', FALSE) + ":" +
          QString::number(minute).rightJustify(2, '0', FALSE) + ":" +
          QString::number(second).rightJustify(2, '0', FALSE) + ":" +
          QString::number(frame).rightJustify(2, '0', FALSE);
	
	return text;
}

int KRulerTimeModel::getTickDisplayInterval(const int tick) const
{
	int seconds = tick;
	
	if(seconds > 3600*numFrames()) {
		int hour = (tick / (3600*numFrames())) + 1;
		seconds = hour * 3600 * numFrames();
	} else if(seconds > 60 * numFrames()) {
		int minute = (tick / (60 * numFrames())) + 1;
		if(minute > 30) {
			minute = 60;
		} else if(minute > 20) {
			minute = 30;
		} else if(minute > 15) {
			minute = 20;
		} else if(minute > 10) {			
			minute = 15;		
		} else if(minute > 5) {
			minute = 10;
		} else if(minute > 1) {
			minute = 5;
		}
		seconds = minute * 60 * numFrames();
	} else {
		seconds /= numFrames();
		seconds++;

		if(seconds > 30) {
			seconds = 60;
		} else if(seconds > 20) {
			seconds = 30;
		} else if(seconds > 15) {
			seconds = 20;
		} else if(seconds > 10) {
			seconds = 15;
		} else if(seconds > 5) {
			seconds = 10;
		} else if(seconds > 2) {
			seconds = 5;
		}

		seconds *= numFrames();
	}

	return seconds;
}

/** Sets the number of frames per second for this ruler model. */
void KRulerTimeModel::setNumFrames(const int frames)
{
	m_numFrames = frames;
}

int KRulerTimeModel::numFrames() const
{
	return m_numFrames;
}
