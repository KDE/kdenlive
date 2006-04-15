/***************************************************************************
                          transitiondialog.cpp  -  description
                             -------------------
    begin                :  Mar 2006
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
#include <cmath>

#include <qnamespace.h>
#include <qhgroupbox.h>
#include <qhbox.h>
#include <qlistbox.h>
#include <qlayout.h>
#include <qslider.h>
#include <qcheckbox.h>

#include <kpushbutton.h>
#include <klocale.h>
#include <kdebug.h>

#include "transition.h"
#include "docclipref.h"
#include "transitiondialog.h"


namespace Gui {

    TransitionDialog::TransitionDialog(QWidget * parent,
                                       const char *name):  QTabWidget(parent), m_transition(0)
{
    transitCrossfade = new transitionCrossfade_UI(this);
    addTab(transitCrossfade, i18n("Crossfade") );

    transitWipe = new transitionWipe_UI(this);
    addTab(transitWipe, i18n("Wipe") );

    transitPip = new transitionPipWidget(240,192,this);
    addTab(transitPip, i18n("PIP") );
    setEnabled(false);
    adjustSize();
}

TransitionDialog::~TransitionDialog() 
{
    disconnectTransition();
    delete transitCrossfade;
    delete transitWipe;
    delete transitPip;
}

void TransitionDialog::setTransition(Transition *transition)
{
        disconnectTransition();
	m_transition = transition;
        if (transition == 0) {
            setEnabled(false);
            return;
        }
        setEnabled(true);
	setActivePage(transition->transitionType());
        setTransitionDirection(transition->invertTransition());
        setTransitionParameters(transition->transitionParameters());
        connectTransition();
}


void TransitionDialog::connectTransition()
{
   connect(this, SIGNAL( currentChanged ( QWidget * )), this, SLOT(applyChanges()));
   connect(transitWipe->transpStart, SIGNAL(sliderReleased ()), this, SLOT(applyChanges()));
   connect(transitWipe->transpEnd, SIGNAL(sliderReleased ()), this, SLOT(applyChanges()));
   connect(transitWipe->transitionDown, SIGNAL(released()), this, SLOT(applyChanges()));
   connect(transitWipe->transitionUp, SIGNAL(released()), this, SLOT(applyChanges()));
   connect(transitWipe->transitionRight, SIGNAL(released()), this, SLOT(applyChanges()));
   connect(transitWipe->transitionLeft, SIGNAL(released()), this, SLOT(applyChanges()));
   connect(transitWipe->invertTransition, SIGNAL(released()), this, SLOT(applyChanges()));
   connect(transitWipe->rescaleImages, SIGNAL(released()), this, SLOT(applyChanges()));
   
   connect(transitCrossfade->invertTransition, SIGNAL(released()), this, SLOT(applyChanges()));
   
   connect(transitPip, SIGNAL(transitionChanged()), this, SLOT(applyChanges()));
}

void TransitionDialog::disconnectTransition()
{
    disconnect(this, SIGNAL( currentChanged ( QWidget * )), this, SLOT(applyChanges()));
    disconnect(transitWipe->transpStart, SIGNAL(sliderReleased ()), this, SLOT(applyChanges()));
    disconnect(transitWipe->transpEnd, SIGNAL(sliderReleased ()), this, SLOT(applyChanges()));
    disconnect(transitWipe->transitionDown, SIGNAL(released()), this, SLOT(applyChanges()));
    disconnect(transitWipe->transitionUp, SIGNAL(released()), this, SLOT(applyChanges()));
    disconnect(transitWipe->transitionRight, SIGNAL(released()), this, SLOT(applyChanges()));
    disconnect(transitWipe->transitionLeft, SIGNAL(released()), this, SLOT(applyChanges()));
    disconnect(transitWipe->invertTransition, SIGNAL(released()), this, SLOT(applyChanges()));
    disconnect(transitWipe->rescaleImages, SIGNAL(released()), this, SLOT(applyChanges()));
    
    disconnect(transitCrossfade->invertTransition, SIGNAL(released()), this, SLOT(applyChanges()));

    disconnect(transitPip, SIGNAL(transitionChanged()), this, SLOT(applyChanges()));
}

bool TransitionDialog::isActiveTransition(Transition *transition)
{
	if (transition == m_transition) return true;
	return false;
}

void TransitionDialog::applyChanges()
{
	if (m_transition) {
		if (m_transition == 0) return;
		m_transition->setTransitionType(selectedTransition());
        	m_transition->setTransitionParameters(transitionParameters());
		m_transition->setTransitionDirection(transitionDirection());
		emit transitionChanged(true);
        }
}

void TransitionDialog::setActivePage(const QString &pageName)
{
    if (pageName == "composite") setCurrentPage(1);
    else if (pageName == "pip") setCurrentPage(2);
    else setCurrentPage(0);
}

QString TransitionDialog::selectedTransition() 
{
    QString pageName = "luma";
    if (currentPageIndex() == 1) pageName = "composite";
    else if (currentPageIndex() == 2) pageName = "pip";
    return pageName;
}

void TransitionDialog::setTransitionDirection(bool direc)
{
    transitCrossfade->invertTransition->setChecked(direc);
    transitWipe->invertTransition->setChecked(direc);
}

void TransitionDialog::setTransitionParameters(const QMap < QString, QString > parameters)
{
    if (currentPageIndex() == 1) {
        // parse the "geometry" argument of MLT's composite transition
        transitWipe->rescaleImages->setChecked(parameters["distort"].toInt());
        QString geom = parameters["geometry"];
        QString geom2 = geom.left(geom.find(";"));
        
        if (geom2.right(4).contains(":",FALSE)!=0) { // Start transparency setting
            int pos = geom2.findRev(":");
            QString last = geom2.right(geom2.length() - pos -1);
            transitWipe->transpStart->setValue(100 - last.toInt());
        }
        
        if (geom.right(4).contains(":",FALSE)!=0) { // Ending transparency setting
            int pos = geom.findRev(":");
            QString last = geom.right(geom.length() - pos -1);
            transitWipe->transpEnd->setValue(100 - last.toInt());
            geom.truncate(pos);
        }
        // Which way does it go ?
        if (geom.endsWith("0%,100%:100%x100%")) transitWipe->transitionDown->setOn(true);
        else if (geom.endsWith("0%,-100%:100%x100%")) transitWipe->transitionUp->setOn(true);
        else if (geom.endsWith("-100%,0%:100%x100%")) transitWipe->transitionLeft->setOn(true);
        else if (geom.endsWith("100%,0%:100%x100%")) transitWipe->transitionRight->setOn(true);
    }
    else if (currentPageIndex() == 2) {
        // parse the "geometry" argument of MLT's composite transition
        //transitWipe->rescaleImages->setChecked(parameters["distort"].toInt());
        transitPip->setParameters(parameters["geometry"]);
        }
}

bool TransitionDialog::transitionDirection()
{
    bool result = true;
    if (currentPageIndex() == 0) result = transitCrossfade->invertTransition->isChecked();
    if (currentPageIndex() == 1) result = transitWipe->invertTransition->isChecked();
    //if (activePageIndex() == 2) result = transitPip->invertTransition->isChecked();
    return result;
}

        
const QMap < QString, QString > TransitionDialog::transitionParameters() 
{
    QMap < QString, QString > paramList;
    if (currentPageIndex() == 0) return paramList; // crossfade
    if (currentPageIndex() == 1) // wipe
    {
        QString startTransparency = QString::null;
        QString endTransparency = QString::null;
        int transp1 = transitWipe->transpStart->value();
        int transp2 = transitWipe->transpEnd->value();
        if (transp1 > 0) startTransparency = ":" + QString::number(100 - transp1);
        if (transp2 > 0) endTransparency = ":" + QString::number(100 - transp2);
        if (transitWipe->transitionDown->isOn()) paramList["geometry"] = "0=0%,0%:100%x100%" + startTransparency + ";-1=0%,100%:100%x100%" + endTransparency;
        else if (transitWipe->transitionUp->isOn()) paramList["geometry"] = "0=0%,0%:100%x100%" + startTransparency + ";-1=0%,-100%:100%x100%" + endTransparency;
        //else if (transitWipe->transitionRight->isOn()) paramList["geometry"] = "0=0%,0%:100%x100%" + startTransparency + ";-1=100%,0%:100%x100%" + endTransparency;
        else if (transitWipe->transitionRight->isOn()) paramList["geometry"] = "0=0,0:720x576" + startTransparency + ";-1=720,0:720x576" + endTransparency;
        else if (transitWipe->transitionLeft->isOn()) paramList["geometry"] = "0=0%,0%:100%x100%" + startTransparency + ";-1=-100%,0%:100%x100%" + endTransparency;
        paramList["progressive"] = "1";
        if (transitWipe->rescaleImages->isChecked()) paramList["distort"] = "1";
        
    }
    else if (currentPageIndex() == 2) // pip
    {
      paramList["geometry"] = transitPip->parameters();
      paramList["progressive"] = "1";
    }
    
    return paramList;
}

} // namespace Gui


