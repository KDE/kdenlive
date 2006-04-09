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

#include <qtooltip.h>
#include <qslider.h>
#include <qtoolbutton.h>

#include <klocale.h>
#include <kiconloader.h>
#include "kmmrulerpanel.h"
#include <kcombobox.h>
#include <kdebug.h>


#include <stdlib.h>
#include <math.h>

namespace Gui {

    const double KMMRulerPanel::maxFrameSize = 100.0;
    const double KMMRulerPanel::expA = 0.9094862739;
    const double KMMRulerPanel::expK = 0.09487537302;
    const int KMMRulerPanel::comboListLength = 14;
    const int KMMRulerPanel::comboScale[] =
	{ 1, 2, 5, 10, 25, 50, 125, 250, 500, 725, 1500, 3000, 6000,
	    12000 };

    KMMRulerPanel::KMMRulerPanel(QWidget * parent,
	const char *name):KMMRulerPanel_UI(parent, name) {
        //QPopupMenu *menu = (QPopupMenu *) m_app->factory()->container("projectlist_context", m_app);
            KIconLoader loader;
            
            menu = new QPopupMenu();
            menu->insertItem(i18n("1 Frame"),0,0);
            menu->insertItem(i18n("2 Frames"),1,1);
            menu->insertItem(i18n("5 Frames"),2,2);
            menu->insertItem(i18n("10 Frames"),3,3);
            menu->insertItem(i18n("1 Second"),4,4);
            menu->insertItem(i18n("2 Seconds"),5,5);
            menu->insertItem(i18n("5 Seconds"),6,6);
            menu->insertItem(i18n("10 Seconds"),7,7);
            menu->insertItem(i18n("20 Seconds"),8,8);
            menu->insertItem(i18n("30 Seconds"),9,9);
            menu->insertItem(i18n("1 Minute"),10,10);
            menu->insertItem(i18n("2 Minutes"),11,11);
            menu->insertItem(i18n("4 Minutes"),12,12);
            menu->insertItem(i18n("8 Minutes"),13,13);
            menu->setCheckable(true);
            zoomButton->setPopup(menu);
            zoomButton->setPopupDelay(-1);
            connect(menu, SIGNAL(activated(int)), this, SLOT(comboScaleChange(int)));
            zoomButton->setIconSet(QIconSet(loader.loadIcon("viewmag", KIcon::Toolbar)));


/*	connect(m_scaleCombo, SIGNAL(activated(int)), this,
            SLOT(comboScaleChange(int)));*/
	connect(m_scaleSlider, SIGNAL(valueChanged(int)), this,
	    SLOT(sliderScaleChange(int)));

	m_sync = false;
    }

    KMMRulerPanel::~KMMRulerPanel() {
    }

/** takes index and figures out the correct scale value from it, which then get's emitted. */
    void KMMRulerPanel::comboScaleChange(int index) {
        uint i=0;
        while (menu->idAt(i)!=-1)
        {
            if (menu->idAt(i) != index) menu->setItemChecked(menu->idAt(i), false);
            else menu->setItemChecked(menu->idAt(i), true);
            i++;
        }
	//m_scaleCombo->setCurrentItem(index);
	if (m_sync) {
	    m_sync = false;
	    return;
	}

	emit timeScaleChanged(maxFrameSize / comboScale[index]);
	m_sync = true;
	m_scaleSlider->setValue((int) (log(comboScale[index] / expA) /
		expK));
	m_sync = false;
    }
    
void KMMRulerPanel::selectedZoom(int value) {
    menu->setItemChecked(value, true);
}

void KMMRulerPanel::changeZoom(bool isUp) {
    int i = selectedMenuItem();
    if (isUp) i = i+1;
    else i = i-1;
    if (i < 0) i = 0;
    if (i > comboListLength - 1) i = comboListLength -1;
    comboScaleChange(i);
}

/** Occurs when the slider changes value, emits a corrected value to provide a non-linear
 (and better) value scaling. */
    void KMMRulerPanel::sliderScaleChange(int value) {
	if (m_sync) {
	    m_sync = false;
	    return;
	}
	//
	// The following values come from solving the following equation :
	//
	// ae^(kn) = C
	//
	// where the values n=1, c=1 and n=100, c=12000 are passed.
	// This is to give us a nice exponential curve for the slider.
	int newValue = (int) (expA * exp(expK * value));

	if (newValue == 0)
	    newValue = 1;
	emit timeScaleChanged(maxFrameSize / newValue);

	m_sync = true;

	int bestValue = -1;
	int bestPos = -1;
	int curValue;

	for (int count = 0; count < comboListLength; count++) {
	    curValue = abs(comboScale[count] - newValue);
	    if ((curValue < bestValue) || (bestPos == -1)) {
		bestValue = curValue;
		bestPos = count;
	    }
	}

	if (bestPos != -1) {
            menu->setItemChecked(menu->idAt(selectedMenuItem()), false);
            menu->setItemChecked(menu->idAt(bestPos), true);
	    //m_scaleCombo->setCurrentItem(bestPos);
	}
	m_sync = false;
    }
    
    int KMMRulerPanel::selectedMenuItem()
    {
        uint i=0;
        while (!menu->isItemChecked(i) && (i < comboListLength)) i++;
        return i;
    }

    void KMMRulerPanel::setScale(double scale) {
	double newScale = maxFrameSize / scale;
	m_scaleSlider->setValue((int) (log(newScale / expA) / expK));
    }

}				// namespace Gui
