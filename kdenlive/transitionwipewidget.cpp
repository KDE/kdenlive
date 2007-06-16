/***************************************************************************
                          transitionWipeWidget  -  description
                             -------------------
    begin                : 2006
    copyright            : (C) 2005 by Jean-Baptiste Mardelle
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

#include <qslider.h>
#include <qspinbox.h>
#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qgroupbox.h>

#include <kpushbutton.h>
#include <kcombobox.h>
#include <kdebug.h>
#include <kstandarddirs.h>
#include <klocale.h>

#include "transitionwipewidget.h"

namespace Gui {

transitionWipeWidget::transitionWipeWidget(QWidget* parent, const char* name, WFlags fl ):
        transitionWipe_UI(parent,name), m_lumaType("lumasPAL")
{
	startTransition = CENTER_TRANSITION;
	endTransition = CENTER_TRANSITION;
	m_startTransparency = 0;
	m_endTransparency = 0;

	connect(transitionU, SIGNAL(clicked()), this, SLOT(updateTransition()));
	connect(transitionUL, SIGNAL(clicked()), this, SLOT(updateTransition()));
	connect(transitionUR, SIGNAL(clicked()), this, SLOT(updateTransition()));
	connect(transitionD, SIGNAL(clicked()), this, SLOT(updateTransition()));
	connect(transitionDR, SIGNAL(clicked()), this, SLOT(updateTransition()));
	connect(transitionDL, SIGNAL(clicked()), this, SLOT(updateTransition()));
	connect(transitionR, SIGNAL(clicked()), this, SLOT(updateTransition()));
	connect(transitionL, SIGNAL(clicked()), this, SLOT(updateTransition()));
	connect(transitionC, SIGNAL(clicked()), this, SLOT(updateTransition()));

	connect(invertTransition, SIGNAL(released()), this, SIGNAL(applyChanges()));

        connect(slider_transparency, SIGNAL(valueChanged(int)), spin_transparency, SLOT(setValue(int)));
        connect(spin_transparency, SIGNAL(valueChanged(int)), slider_transparency, SLOT(setValue(int)));
	connect(spin_transparency, SIGNAL(valueChanged(int)), this, SLOT(updateTransparency(int)));
        connect(key_start, SIGNAL(stateChanged(int)), this, SLOT(updateButtons()));

        connect(spin_soft, SIGNAL(valueChanged(int)), slider_soft, SLOT(setValue(int)));
	connect(slider_soft, SIGNAL(valueChanged(int)), spin_soft, SLOT(setValue(int)));
	connect(spin_soft, SIGNAL(valueChanged(int)), this, SIGNAL(applyChanges()));
	connect(luma_file, SIGNAL(activated(int)), this, SIGNAL(applyChanges()));
}


transitionWipeWidget::~transitionWipeWidget()
{
}

QMap < QString, QString > transitionWipeWidget::parameters()
{
    QMap < QString, QString > paramList;
    QString geom = "0=";
    switch (startTransition) {
	case CENTER_TRANSITION:
	geom += "0%,0%";
	break;
	case LEFT_TRANSITION:
	geom += "-100%,0%";
	break;
	case RIGHT_TRANSITION:
	geom += "100%,0%";
	break;
	case TOPLEFT_TRANSITION:
	geom += "-100%,-100%";
	break;
	case BOTTOMLEFT_TRANSITION:
	geom += "-100%,100%";
	break;
	case TOPRIGHT_TRANSITION:
	geom += "100%,-100%";
	break;
	case BOTTOMRIGHT_TRANSITION:
	geom += "100%,100%";
	break;
	case TOP_TRANSITION:
	geom += "0%,-100%";
	break;
	case BOTTOM_TRANSITION:
	geom += "0%,100%";
	break;
    }
    geom += ":100%x100%:" + QString::number(100 - m_startTransparency) + ";-1=";
    switch (endTransition) {
	case CENTER_TRANSITION:
	geom += "0%,0%";
	break;
	case LEFT_TRANSITION:
	geom += "-100%,0%";
	break;
	case RIGHT_TRANSITION:
	geom += "100%,0%";
	break;
	case TOPLEFT_TRANSITION:
	geom += "-100%,-100%";
	break;
	case BOTTOMLEFT_TRANSITION:
	geom += "-100%,100%";
	break;
	case TOPRIGHT_TRANSITION:
	geom += "100%,-100%";
	break;
	case BOTTOMRIGHT_TRANSITION:
	geom += "100%,100%";
	break;
	case TOP_TRANSITION:
	geom += "0%,-100%";
	break;
	case BOTTOM_TRANSITION:
	geom += "0%,100%";
	break;
    }
    geom += ":100%x100%:" + QString::number(100 - m_endTransparency);
    paramList["geometry"] = geom;
    paramList["progressive"] = "1";

    if (use_luma->isChecked()) {
	QString fname = luma_file->currentText();
	fname = locate(m_lumaType, fname + ".pgm");
	paramList["luma"] = fname;
	paramList["softness"] = QString::number(((double) spin_soft->value()) / 100.0);
    }

    return paramList;
}

void transitionWipeWidget::setFormat(char *format)
{
	m_lumaType = format;
}

void transitionWipeWidget::setParameters(QString geom)
{
	QString geom2 = geom.left(geom.find(";"));
	geom = geom.section(";",1,1);

        if (geom2.right(4).contains(":",FALSE)!=0) { // Start transparency setting
            int pos = geom2.findRev(":");
            QString last = geom2.right(geom2.length() - pos -1);
            m_startTransparency = 100 - last.toInt();
        }


        if (geom.right(4).contains(":",FALSE)!=0) { // Ending transparency setting
            int pos = geom.findRev(":");
            QString last = geom.right(geom.length() - pos -1);
            m_endTransparency = 100 - last.toInt();
	    geom.truncate(pos);
        }


	if (geom2.startsWith("0=0%,0%")) startTransition = CENTER_TRANSITION;
	else if (geom2.startsWith("0=-100%,0%")) startTransition = LEFT_TRANSITION;
	else if (geom2.startsWith("0=100%,0%")) startTransition = RIGHT_TRANSITION;
	else if (geom2.startsWith("0=-100%,-100%")) startTransition = TOPLEFT_TRANSITION;
	else if (geom2.startsWith("0=-100%,100%")) startTransition = BOTTOMLEFT_TRANSITION;
	else if (geom2.startsWith("0=100%,-100%")) startTransition = TOPRIGHT_TRANSITION;
	else if (geom2.startsWith("0=100%,100%")) startTransition = BOTTOMRIGHT_TRANSITION;
	else if (geom2.startsWith("0=0%,-100%")) startTransition = TOP_TRANSITION;
	else if (geom2.startsWith("0=0%,100%")) startTransition = BOTTOM_TRANSITION;

	if (geom.endsWith("=0%,0%:100%x100%")) endTransition = CENTER_TRANSITION;
	else if (geom.endsWith("=-100%,0%:100%x100%")) endTransition = LEFT_TRANSITION;
	else if (geom.endsWith("=100%,0%:100%x100%")) endTransition = RIGHT_TRANSITION;
	else if (geom.endsWith("=-100%,-100%:100%x100%")) endTransition = TOPLEFT_TRANSITION;
	else if (geom.endsWith("=-100%,100%:100%x100%")) endTransition = BOTTOMLEFT_TRANSITION;
	else if (geom.endsWith("=100%,-100%:100%x100%")) endTransition = TOPRIGHT_TRANSITION;
	else if (geom.endsWith("=100%,100%:100%x100%")) endTransition = BOTTOMRIGHT_TRANSITION;
	else if (geom.endsWith("=0%,-100%:100%x100%")) endTransition = TOP_TRANSITION;
	else if (geom.endsWith("=0%,100%:100%x100%")) endTransition = BOTTOM_TRANSITION;
	updateButtons();
}

void transitionWipeWidget::resetTransition()
{
    invertTransition->setChecked(false);
    key_start->setChecked(true);
    transitionC->setOn(true);
    slider_transparency->setValue(0);
    startTransition = CENTER_TRANSITION;
    endTransition = CENTER_TRANSITION;
    m_startTransparency = 0;
    m_endTransparency = 0;
}

void transitionWipeWidget::updateTransparency(int transp)
{
    if (key_start->isChecked()) m_startTransparency = transp;
    else m_endTransparency = transp;
    emit applyChanges();
}

void transitionWipeWidget::updateTransition()
{
    TRANSITIONWIPETYPE trans;
    if (transitionC->isOn()) trans = CENTER_TRANSITION;
    else if (transitionU->isOn()) trans = TOP_TRANSITION;
    else if (transitionUL->isOn()) trans = TOPLEFT_TRANSITION;
    else if (transitionUR->isOn()) trans = TOPRIGHT_TRANSITION;
    else if (transitionD->isOn()) trans = BOTTOM_TRANSITION;
    else if (transitionDL->isOn()) trans = BOTTOMLEFT_TRANSITION;
    else if (transitionDR->isOn()) trans = BOTTOMRIGHT_TRANSITION;
    else if (transitionR->isOn()) trans = RIGHT_TRANSITION;
    else if (transitionL->isOn()) trans = LEFT_TRANSITION;

    if (key_start->isChecked()) startTransition = trans;
    else endTransition = trans;
    emit applyChanges();
}

void transitionWipeWidget::updateButtons()
{
    TRANSITIONWIPETYPE trans;
    if (key_start->isChecked()) {
	trans = startTransition;
	slider_transparency->setValue(m_startTransparency);
    }
    else {
	trans = endTransition;
	slider_transparency->setValue(m_endTransparency);
    }

    switch (trans) {
	case CENTER_TRANSITION:
	transitionC->setOn(true);
	break;
	case TOP_TRANSITION:
	transitionU->setOn(true);
	break;
	case BOTTOM_TRANSITION:
	transitionD->setOn(true);
	break;
	case TOPLEFT_TRANSITION:
	transitionUL->setOn(true);
	break;
	case TOPRIGHT_TRANSITION:
	transitionUR->setOn(true);
	break;
	case BOTTOMLEFT_TRANSITION:
	transitionDL->setOn(true);
	break;
	case BOTTOMRIGHT_TRANSITION:
	transitionDR->setOn(true);
	break;
	case LEFT_TRANSITION:
	transitionL->setOn(true);
	break;
	case RIGHT_TRANSITION:
	transitionR->setOn(true);
	break;
    }
}


}  //  end GUI namespace
