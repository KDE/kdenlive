/***************************************************************************
                          kmmrulerpanel.cpp  -  description
                             -------------------
    begin                : Sat Sep 14 2002
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

#include "kmmrulerpanel.h"
#include "kcombobox.h"
#include "qslider.h"
 
int KMMRulerPanel::comboScale[] = {1, 2, 5, 10, 25, 50, 125, 250, 500, 725, 1500, 3000, 6000, 12000};

KMMRulerPanel::KMMRulerPanel(QWidget *parent, const char *name ) : KMMRulerPanel_UI(parent,name)
{
	connect(m_scaleCombo, SIGNAL(activated(int)), this, SLOT(comboScaleChange(int)));
	connect(m_scaleSlider, SIGNAL(valueChanged(int)), this, SIGNAL(timeScaleChanged(int)));
}

KMMRulerPanel::~KMMRulerPanel()
{
}

/** takes index and figures out the correct scale value from it, which then get's emitted. */
void KMMRulerPanel::comboScaleChange(int index)
{
	emit timeScaleChanged(comboScale[index]);
}
