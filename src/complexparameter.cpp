/***************************************************************************
                          complexparameter.cpp  -  description
                             -------------------
    begin                : Feb 15 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <KDebug>
#include <KLocale>

#include "complexparameter.h"
#include "effectslist.h"
#include "clipitem.h"
#include <QHeaderView>
#include <QMenu>

ComplexParameter::ComplexParameter(QWidget *parent)
: QWidget(parent)
{
	ui.setupUi(this);
	//ui.effectlist->horizontalHeader()->setVisible(false);
	//ui.effectlist->verticalHeader()->setVisible(false);
	activeRow=-1;
	clipref=NULL;

	
	ui.buttonLeftRight->setIcon(KIcon("go-next"));//better icons needed
	ui.buttonLeftRight->setToolTip(i18n("Allow horizontal moves"));
	ui.buttonUpDown->setIcon(KIcon("go-up"));
	ui.buttonUpDown->setToolTip(i18n("Allow vertical moves"));
	ui.buttonShowInTimeline->setIcon(KIcon("kmplayer"));
	ui.buttonShowInTimeline->setToolTip(i18n("Show keyframes in timeline"));
	ui.buttonHelp->setIcon(KIcon("help-about"));
	ui.buttonHelp->setToolTip(i18n("Parameter info"));
	ui.buttonNewPoints->setIcon(KIcon("xedit"));
	ui.buttonNewPoints->setToolTip(i18n("Add keyframe"));
	
	connect (ui.buttonLeftRight, SIGNAL (clicked()), this , SLOT ( slotSetMoveX() ) );
	connect (ui.buttonUpDown, SIGNAL (clicked()), this , SLOT ( slotSetMoveY() ) );
	connect (ui.buttonShowInTimeline, SIGNAL (clicked()), this , SLOT ( slotShowInTimeline() ) );
	connect (ui.buttonNewPoints, SIGNAL (clicked()), this , SLOT ( slotSetNew() ) );
	connect (ui.buttonHelp, SIGNAL (clicked()), this , SLOT ( slotSetHelp() ) );
	connect (ui.parameterList, SIGNAL (currentIndexChanged ( const QString &  ) ), this, SLOT( slotParameterChanged(const QString&) ) );
	//connect (ui.effectlist, SIGNAL (itemSelectionChanged() ) , this, SLOT ( itemSelectionChanged()));
	connect( this,SIGNAL (transferParamDesc(const QDomElement&,int ,int) ), ui.kplotwidget, SLOT(setPointLists(const QDomElement&,int ,int) ));
	connect(ui.kplotwidget, SIGNAL (parameterChanged(QDomElement ) ), this , SLOT (slotUpdateEffectParams(QDomElement)));
	/*ÜeffectLists["audio"]=audioEffectList;
	effectLists["video"]=videoEffectList;
	effectLists["custom"]=customEffectList;*/
	
	ui.infoBox->hide();	
	updateButtonStatus();
	
}



void ComplexParameter::slotSetMoveX(){
	ui.kplotwidget->setMoveX(!ui.kplotwidget->isMoveX());
	updateButtonStatus();
}

void ComplexParameter::slotSetMoveY(){
	ui.kplotwidget->setMoveY(!ui.kplotwidget->isMoveY());
	updateButtonStatus();
}

void ComplexParameter::slotSetNew(){
	ui.kplotwidget->setNewPoints(!ui.kplotwidget->isNewPoints());
	updateButtonStatus();
}

void ComplexParameter::slotSetHelp(){
	ui.infoBox->setVisible(!ui.infoBox->isVisible());
	ui.buttonHelp->setDown(ui.infoBox->isVisible());
}

void ComplexParameter::slotShowInTimeline(){

	ui.kplotwidget->setMoveTimeLine(!ui.kplotwidget->isMoveTimeline());
	updateButtonStatus();
	
}

void ComplexParameter::updateButtonStatus(){
	ui.buttonLeftRight->setDown(ui.kplotwidget->isMoveX());
	ui.buttonUpDown->setDown(ui.kplotwidget->isMoveY());
	
	ui.buttonShowInTimeline->setEnabled( ui.kplotwidget->isMoveX() || ui.kplotwidget->isMoveY ()  );
	ui.buttonShowInTimeline->setDown(ui.kplotwidget->isMoveTimeline());
	
	ui.buttonNewPoints->setEnabled(ui.parameterList->currentText()!="all");
	ui.buttonNewPoints->setDown(ui.kplotwidget->isNewPoints());
}

void ComplexParameter::slotParameterChanged(const QString& text){
	
	//ui.buttonNewPoints->setEnabled(text!="all");
	ui.kplotwidget->replot(text);
	updateButtonStatus();
}

void ComplexParameter::setupParam(const QDomElement& d,int from,int to){
	ui.kplotwidget->setPointLists(d,from,to);
}

void ComplexParameter::itemSelectionChanged (){
	//kDebug() << "drop";
}
#include "complexparameter.moc"
