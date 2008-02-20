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
#include "effectslist.h"
#include "clipitem.h"
#include <QHeaderView>
#include <QMenu>

EffectStackView::EffectStackView(EffectsList *audioEffectList, EffectsList *videoEffectList, EffectsList *customEffectList, QWidget *parent)
: QWidget(parent)
{
	ui.setupUi(this);
	//ui.effectlist->horizontalHeader()->setVisible(false);
	//ui.effectlist->verticalHeader()->setVisible(false);
	activeRow=-1;
	clipref=NULL;
	
	ui.buttonNew->setIcon(KIcon("document-new"));
	ui.buttonUp->setIcon(KIcon("go-up"));
	ui.buttonDown->setIcon(KIcon("go-down"));
	ui.buttonDel->setIcon(KIcon("trash-empty"));
	
	ui.buttonLeftRight->setIcon(KIcon("go-next"));//better icons needed
	ui.buttonUpDown->setIcon(KIcon("go-up"));
	ui.buttonShowInTimeline->setIcon(KIcon("kmplayer"));
	ui.buttonHelp->setIcon(KIcon("help-about"));
	ui.buttonNewPoints->setIcon(KIcon("xedit"));
	
	ui.effectlist->setDragDropMode(QAbstractItemView::NoDragDrop);//use internal if drop is recognised right
	
	connect (ui.effectlist, SIGNAL ( itemSelectionChanged()), this , SLOT( slotItemSelectionChanged() ));
	connect (ui.buttonNew, SIGNAL (clicked()), this, SLOT (slotNewEffect()) );
	connect (ui.buttonUp, SIGNAL (clicked()), this, SLOT (slotItemUp()) );
	connect (ui.buttonDown, SIGNAL (clicked()), this, SLOT (slotItemDown()) );
	connect (ui.buttonDel, SIGNAL (clicked()), this, SLOT (slotItemDel()) );
	connect (ui.buttonLeftRight, SIGNAL (clicked()), this , SLOT ( slotSetMoveX() ) );
	connect (ui.buttonUpDown, SIGNAL (clicked()), this , SLOT ( slotSetMoveY() ) );
	connect (ui.buttonShowInTimeline, SIGNAL (clicked()), this , SLOT ( slotShowInTimeline() ) );
	connect (ui.buttonNewPoints, SIGNAL (clicked()), this , SLOT ( slotSetNew() ) );
	connect (ui.buttonHelp, SIGNAL (clicked()), this , SLOT ( slotSetHelp() ) );
	connect (ui.parameterList, SIGNAL (currentIndexChanged ( const QString &  ) ), this, SLOT( slotParameterChanged(const QString&) ) );
	connect (ui.effectlist, SIGNAL (itemSelectionChanged() ) , this, SLOT ( itemSelectionChanged()));
	
	effectLists["audio"]=audioEffectList;
	effectLists["video"]=videoEffectList;
	effectLists["custom"]=customEffectList;
	
	ui.infoBox->hide();	
	updateButtonStatus();
	
	
	QList< QPair<QString, QMap<int,QVariant> > > points;
	QMap<int,QVariant> data;
	data[0]=0.1;
	data[100]=30;
	data[255]=50;
	data[300]=100;
	QPair<QString,QMap<int,QVariant> > testpair("gray",data);
	points.append(testpair);
	
	QMap<int,QVariant> data1;
	data1[0]=20;
	data1[10]=70;
	data1[155]=110;
	data1[300]=133;
	QPair<QString,QMap<int,QVariant> > testpair1("dx",data1);
	points.append(testpair1);
	ui.parameterList->addItem("all");
	ui.parameterList->addItem("gray");
	ui.parameterList->addItem("dx");
	
	ui.kplotwidget->setPointLists(points,0,305);
	
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
		if (activeRow==i){
			item->setSelected(true);
			ui.effectlist->setCurrentRow(activeRow);
		}
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

void EffectStackView::slotSetMoveX(){
	ui.kplotwidget->setMoveX(!ui.kplotwidget->isMoveX());
	updateButtonStatus();
}

void EffectStackView::slotSetMoveY(){
	ui.kplotwidget->setMoveY(!ui.kplotwidget->isMoveY());
	updateButtonStatus();
}

void EffectStackView::slotSetNew(){
	ui.kplotwidget->setNewPoints(!ui.kplotwidget->isNewPoints());
	updateButtonStatus();
}

void EffectStackView::slotSetHelp(){
	ui.infoBox->setVisible(!ui.infoBox->isVisible());
	ui.buttonHelp->setDown(ui.infoBox->isVisible());
}

void EffectStackView::slotShowInTimeline(){

	ui.kplotwidget->setMoveTimeLine(!ui.kplotwidget->isMoveTimeline());
	updateButtonStatus();
	
}

void EffectStackView::updateButtonStatus(){
	ui.buttonLeftRight->setDown(ui.kplotwidget->isMoveX());
	ui.buttonUpDown->setDown(ui.kplotwidget->isMoveY());
	
	ui.buttonShowInTimeline->setEnabled( ui.kplotwidget->isMoveX() || ui.kplotwidget->isMoveY ()  );
	ui.buttonShowInTimeline->setDown(ui.kplotwidget->isMoveTimeline());
	
	ui.buttonNewPoints->setEnabled(ui.parameterList->currentText()!="all");
	ui.buttonNewPoints->setDown(ui.kplotwidget->isNewPoints());
}

void EffectStackView::slotParameterChanged(const QString& text){
	
	//ui.buttonNewPoints->setEnabled(text!="all");
	updateButtonStatus();
}

void EffectStackView::slotNewEffect(){
	

	QMenu *displayMenu=new QMenu (this);
	displayMenu->setTitle("Filters");
	foreach (QString type, effectLists.keys() ){
		QAction *a=new QAction(type,displayMenu);
		EffectsList *list=effectLists[type];

		QMenu *parts=new QMenu(type,displayMenu);
		parts->setTitle(type);
		foreach (QString name, list->effectNames()){
			QAction *entry=new QAction(name,parts);
			entry->setData(name);
			entry->setToolTip(list->getInfo(name));
			entry->setStatusTip(list->getInfo(name));
			parts->addAction(entry);
			//QAction
		}
		displayMenu->addMenu(parts);

	}

	QAction *result=displayMenu->exec(mapToGlobal(ui.buttonNew->pos()+ui.buttonNew->rect().bottomRight()));
	
	if (result){
		effects.append(result->data().toString());
		setupListView(effects);
		kDebug()<< result->data();
	}
	delete displayMenu;
	
}

void EffectStackView::itemSelectionChanged (){
	//kDebug() << "drop";
}
#include "effectstackview.moc"
