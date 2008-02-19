/***************************************************************************
                          effecstackview.cpp  -  description
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

#include "effectstackview.h"
#include "clipitem.h"
#include <QHeaderView>

EffectStackView::EffectStackView( QWidget *parent)
: QWidget(parent)
{
	ui.setupUi(this);
	//ui.effectlist->horizontalHeader()->setVisible(false);
	//ui.effectlist->verticalHeader()->setVisible(false);
	activeRow=-1;
	clipref=NULL;
	connect (ui.effectlist, SIGNAL ( itemSelectionChanged()), this , SLOT( slotItemSelectionChanged() ));
	connect (ui.buttonUp, SIGNAL (clicked()), this, SLOT (slotItemUp()) );
	connect (ui.buttonDown, SIGNAL (clicked()), this, SLOT (slotItemDown()) );
	connect (ui.buttonDel, SIGNAL (clicked()), this, SLOT (slotItemDel()) );
	

	
	QList< QPair<QString, QMap<int,QVariant> > > points;
	QMap<int,QVariant> data;
	data[0]=0.1;
	data[100]=50;
	data[255]=100;
	QPair<QString,QMap<int,QVariant> > testpair("contrast",data);
	points.append(testpair);
	ui.kplotwidget->setPointLists(points,0,300);
	
}

void EffectStackView::slotClipItemSelected(ClipItem* c)
{
	clipref=c;
	if (clipref==NULL)
		return;
	effects=clipref->effectNames();
	setupListView(effects);
	
}

void EffectStackView::setupListView(const QStringList& effects_list){

	ui.effectlist->clear();
	ui.effectlist->addItems(effects);
	for (int i=0;i< ui.effectlist->count();i++){
		QListWidgetItem* item=ui.effectlist->item(i);
		item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsDragEnabled|Qt::ItemIsUserCheckable|Qt::ItemIsEnabled);
		item->setCheckState(Qt::Checked);
		if (activeRow==i)
			item->setSelected(true);
	}

}

void EffectStackView::slotItemSelectionChanged(){
	
	if (ui.effectlist->currentItem() && ui.effectlist->currentItem()->isSelected() ){
		activeRow=ui.effectlist->row( ui.effectlist->currentItem() );
	}else{
		activeRow=-1;
	}
	ui.buttonDel->setEnabled( activeRow!=-1);
	ui.buttonUp->setEnabled( activeRow >0 );
	ui.buttonDown->setEnabled( (activeRow<ui.effectlist->count()-1) && activeRow>=0 );
}

void EffectStackView::slotItemUp(){
	if (activeRow>0 && activeRow <effects.size() ){
		effects.swap(activeRow, activeRow-1);
	}
	activeRow--;
	setupListView(effects);
	
}

void EffectStackView::slotItemDown(){
	if (activeRow<effects.size()-1  ){
		effects.swap(activeRow, activeRow+1);
	}
	activeRow++;
	setupListView(effects);
	
}

void EffectStackView::slotItemDel(){
	if (activeRow<effects.size() && activeRow>=0  ){
		effects.removeAt(activeRow);
	}
	if (effects.size()>0 && activeRow>0)
	activeRow--;
	setupListView(effects);
	
}

#include "effectstackview.moc"
