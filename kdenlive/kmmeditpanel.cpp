/***************************************************************************
                          kmmeditpanelimplementation.cpp  -  description
                             -------------------
    begin                : Mon Apr 8 2002
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
 
#include "kmmeditpanel.h"

#include "kiconloader.h"
#include "qtoolbutton.h"

#include "kfixedruler.h"
#include "krulertimemodel.h"

KMMEditPanel::KMMEditPanel(QWidget* parent, const char* name, WFlags fl) :
																 KMMEditPanel_UI(parent, name, fl)
{
	m_ruler->addSlider(KRuler::Diamond, 0);
	m_ruler->setRulerModel(new KRulerTimeModel());
	m_ruler->setRange(0, 1500);
	m_ruler->setMargin(40);

	KIconLoader loader;

	startButton->setPixmap(loader.loadIcon("player_start", KIcon::Toolbar));
	rewindButton->setPixmap(loader.loadIcon("player_rew", KIcon::Toolbar));
	stopButton->setPixmap(loader.loadIcon("player_stop", KIcon::Toolbar));
	playButton->setPixmap(loader.loadIcon("player_play", KIcon::Toolbar));
	forwardButton->setPixmap(loader.loadIcon("player_fwd", KIcon::Toolbar));
	endButton->setPixmap(loader.loadIcon("player_end", KIcon::Toolbar));

	connect(m_ruler, SIGNAL(sliderValueChanged(int, int)), this, SLOT(rulerValueChanged(int, int)));
}

KMMEditPanel::~KMMEditPanel()
{
}

/** Sets the length of the clip that we are viewing. */
void KMMEditPanel::setClipLength(int frames)
{
	m_ruler->setMaxValue(frames);
}

/** A slider on the ruler has changed value */
void KMMEditPanel::rulerValueChanged(int ID, int value)
{
	if(ID == 0) {
		emit seekPositionChanged(GenTime(value, 25));
	}
}
