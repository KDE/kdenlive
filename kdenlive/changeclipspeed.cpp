/***************************************************************************
                          createslideshowclip.cpp  -  description
                             -------------------
    begin                :  Jul 2006
    copyright            : (C) 2006 by Jean-Baptiste Mardelle
    email                : jb@ader.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qspinbox.h>
#include <qslider.h>
#include <qpushbutton.h>


#include <klocale.h>
#include <kdebug.h>


#include "timecode.h"
#include "kdenlivesettings.h"
#include "changeclipspeed.h"


namespace Gui {

    changeClipSpeed::changeClipSpeed(int speed, GenTime duration, QWidget * parent, const char *name):  changeClipSpeed_UI (parent,name), m_duration(duration)  {

	connect(speedSlider, SIGNAL(valueChanged(int)), speedBox, SLOT(setValue(int)));
        connect(speedBox, SIGNAL(valueChanged(int)), speedSlider, SLOT(setValue(int)));

	connect(speedSlider, SIGNAL(valueChanged(int)), this, SLOT(updateDuration()));
	connect(buttonReset, SIGNAL(clicked()), this, SLOT(resetSpeed()));
	speedBox->setValue(speed);
	updateDuration();
	
    }

    changeClipSpeed::~changeClipSpeed()
    {}

    GenTime changeClipSpeed::duration()
    {
        QString d = edit_duration->text();
        int frames = (d.section(":",0,0).toInt()*3600 + d.section(":",1,1).toInt()*60 + d.section(":",2,2).toInt()) * KdenliveSettings::defaultfps() + d.section(":",3,3).toInt();
        return GenTime(frames , KdenliveSettings::defaultfps());
    }

    void changeClipSpeed::updateDuration()
    {
	Timecode tcode;
	GenTime newDuration(m_duration.frames(KdenliveSettings::defaultfps()) / selectedSpeed(), KdenliveSettings::defaultfps());
        edit_duration->setText(tcode.getTimecode(newDuration, KdenliveSettings::defaultfps()));
    }

    void changeClipSpeed::resetSpeed()
    {
	speedBox->setValue(100);
    }

    double changeClipSpeed::selectedSpeed()
    {
	return ((double) speedBox->value()) / 100.0;
    }

} // namespace Gui


