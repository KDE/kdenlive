/***************************************************************************
                          kmmruler.cpp  -  description
                             -------------------
    begin                : Fri Feb 15 2002
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

#include <qpainter.h>
#include <qcolor.h>
#include <qtextstream.h>

#include "kmmruler.h"

KMMRuler::KMMRuler(QWidget *parent, const char *name ) :
						QWidget(parent,name),
						_sizehint(500, 32)
{	
//	timeScale = KTS_FRAME;
	timeScale = KTS_HMSF;
	
	framesPerSecond = 25;	
	setFrameSize(0.01);
	leftMostPixel = 0;
	
	setMinimumHeight(32);
	setMinimumWidth(32);
	setMaximumHeight(32);
	
	setSizePolicy(QSizePolicy (QSizePolicy::Expanding, QSizePolicy::Fixed, FALSE));
  setBackgroundMode(Qt::NoBackground);
}

KMMRuler::~KMMRuler(){
}
/** No descriptions */

void KMMRuler::paintEvent(QPaintEvent *event) {
	QPainter painter(this);
	int frame;
	double pixel;
	double pixelIncrement;
	int sx = event->rect().x();
	int ex = sx + event->rect().width();
	

	painter.fillRect(event->rect(), QColor(255, 255, 255));

	//
	// Draw small ticks
	//
	
	frame = (int)((leftMostPixel+sx) / xFrameSize);
	frame -= frame % smallTickEvery;
	pixel = (frame * xFrameSize) - leftMostPixel;
	pixelIncrement = xFrameSize * smallTickEvery;

	while(pixel < ex) {
		painter.drawLine((int)pixel, 0, (int)pixel, 4);
		painter.drawLine((int)pixel, 28, (int)pixel, 32);		
		frame += smallTickEvery;
		pixel += pixelIncrement;		
	}			
	
	//
	// Draw Big ticks
	//
	frame = (int)((leftMostPixel+sx) / xFrameSize);
	frame -= frame % bigTickEvery;
	pixel = (frame * xFrameSize) - leftMostPixel;
	pixelIncrement = xFrameSize * bigTickEvery;

	while(pixel < ex) {
		painter.drawLine((int)pixel, 0, (int)pixel, 8);
		painter.drawLine((int)pixel, 24, (int)pixel, 32);		
		frame += bigTickEvery;
		pixel += pixelIncrement;		
	}		
	
	//
	// Draw displayed text
	//
	
	frame = (int)((leftMostPixel + sx) / xFrameSize);
	frame -= frame % textEvery;
	pixel = (frame * xFrameSize) - leftMostPixel;
	pixelIncrement = xFrameSize * textEvery;

	while(pixel < ex) {	
//		painter.drawLine((int)pixel, 0, (int)pixel, 8);	
//		painter.drawLine((int)pixel, 24, (int)pixel, 32);	
		painter.drawText((int)pixel-50, 0, 100, 32, AlignCenter, getTimeScaleText(frame));
		frame += textEvery;
		pixel += pixelIncrement;		
	}	
}

QSize KMMRuler::sizeHint() {
	return _sizehint;
}

void KMMRuler::setValue(int value) {
	leftMostPixel = value;
	update();
}

/** No descriptions */
void KMMRuler::setFrameSize(double size){	
	xFrameSize = size;
		
	textEvery = getTickCount(100.0);
	bigTickEvery = getTickCount(10.0);
	smallTickEvery = getTickCount(3.0);
	
}

int KMMRuler::getTickCount(double size)
{
	int tick=1;	
	
	while (tick*xFrameSize < size) tick *= 2;
	
	if(tick > framesPerSecond) {
		int seconds = tick/framesPerSecond;
		
		if(seconds > 60) {
			int minutes = seconds / 60;
			if(minutes > 60) {
				int hours = minutes/60;
				minutes = hours*60;
			} else if(minutes > 30) {
				minutes = 60;
			} else if(minutes > 20) {
				minutes = 30;
			} else if(minutes > 10) {
				minutes = 20;
			} else if(minutes > 5) {
				minutes = 10;
			} else if(minutes > 2) {
				minutes = 5;
			} else if(minutes > 1) {
				minutes = 2;
			}
			seconds = minutes * 60;
		} else if(seconds > 30) {
			seconds = 60;
		} else if(seconds > 20) {
			seconds = 30;
		} else if(seconds > 10) {
			seconds = 20;
		} else if(seconds > 5) {
			seconds = 10;
		} else if(seconds > 2) {
			seconds = 5;
		} else {
			seconds = 1;
		}		
		tick = seconds*framesPerSecond;
	} else {
		while((tick < framesPerSecond) && (framesPerSecond % tick != 0)) tick++;
	}
	
	return tick;	
}

QString KMMRuler::getTimeScaleText(int frame) {
	QString text;
	int hour, minute, second;

	switch(timeScale) {
		case KTS_HMSX : hour = frame / (3600*framesPerSecond);
										frame -= hour * (3600*framesPerSecond);
										minute = frame / (60 * framesPerSecond);
										frame -= minute * (60*framesPerSecond);
										second = frame / framesPerSecond;
										frame -= second * framesPerSecond;
										frame = (int)((frame * 100.0)/framesPerSecond);
										QTextOStream(&text) << hour << ":" << minute << ":" << second << "." << frame;
										break;
		case KTS_HMSF :
										hour = frame / (3600*framesPerSecond);
										frame -= hour * (3600*framesPerSecond);
										minute = frame / (60 * framesPerSecond);
										frame -= minute * (60*framesPerSecond);
										second = frame / framesPerSecond;
										frame -= second * framesPerSecond;
										QTextOStream(&text) << hour << ":" << minute << ":" << second << ":" << frame;										
										break;
		case KTS_FRAME :
										QTextOStream(&text) << frame;
										break;
	}
	
	return text;
}
