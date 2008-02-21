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
	effectedit=new EffectStackEdit(ui.groupBox,this);
	//ui.effectlist->horizontalHeader()->setVisible(false);
	//ui.effectlist->verticalHeader()->setVisible(false);
	activeRow=-1;
	clipref=NULL;
	
	ui.buttonNew->setIcon(KIcon("document-new"));
	ui.buttonNew->setToolTip(i18n("Add new effect"));
	ui.buttonUp->setIcon(KIcon("go-up"));
	ui.buttonUp->setToolTip(i18n("Move effect up"));
	ui.buttonDown->setIcon(KIcon("go-down"));
	ui.buttonDown->setToolTip(i18n("Move effect down"));
	ui.buttonDel->setIcon(KIcon("trash-empty"));
	ui.buttonDel->setToolTip(i18n("Delete effect"));
	

	
	ui.effectlist->setDragDropMode(QAbstractItemView::NoDragDrop);//use internal if drop is recognised right
	
	connect (ui.effectlist, SIGNAL ( itemSelectionChanged()), this , SLOT( slotItemSelectionChanged() ));
	connect (ui.buttonNew, SIGNAL (clicked()), this, SLOT (slotNewEffect()) );
	connect (ui.buttonUp, SIGNAL (clicked()), this, SLOT (slotItemUp()) );
	connect (ui.buttonDown, SIGNAL (clicked()), this, SLOT (slotItemDown()) );
	connect (ui.buttonDel, SIGNAL (clicked()), this, SLOT (slotItemDel()) );
	connect( this,SIGNAL (transferParamDesc(const QDomElement&,int ,int) ), effectedit , SLOT(transferParamDesc(const QDomElement&,int ,int)));
	connect(effectedit, SIGNAL (parameterChanged(const QDomElement&  ) ), this , SLOT (slotUpdateEffectParams(const QDomElement&)));
	effectLists["audio"]=audioEffectList;
	effectLists["video"]=videoEffectList;
	effectLists["custom"]=customEffectList;
	
	ui.infoBox->hide();	
	
	
}

void EffectStackView::slotUpdateEffectParams(const QDomElement& e){
	effects[activeRow]=e;
	if (clipref)
		emit updateClipEffect(clipref, e);
}

void EffectStackView::slotClipItemSelected(ClipItem* c)
{
	clipref=c;
	if (clipref==NULL) {
		setEnabled(false);
		return;
	}
	setEnabled(true);
	//effects=clipref->effectNames();
	effects.clear();
	for (int i=0;i<clipref->effectsCount();i++){
		QString outstr;
		QTextStream str(&outstr);
		clipref->effectAt(i).save(str,2);
		kDebug() << outstr;
		effects.append(clipref->effectAt(i));
	}
	setupListView();
	
}

void EffectStackView::setupListView(){

	ui.effectlist->clear();
	foreach (QDomElement d,effects){
		
		QDomNode namenode = d.elementsByTagName("name").item(0);
		if (!namenode.isNull()) 
			ui.effectlist->addItem(namenode.toElement().text() );
	}
	//ui.effectlist->addItems(effects);
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
		emit transferParamDesc(effects.at(activeRow) ,0,100);//minx max frame
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
	setupListView();
	
}

void EffectStackView::slotItemDown(){
	if (activeRow<effects.size()-1  ){
		effects.swap(activeRow, activeRow+1);
	}
	activeRow++;
	setupListView();
	
}

void EffectStackView::slotItemDel(){
	if (activeRow<effects.size() && activeRow>=0  ){
		emit removeEffect(clipref, clipref->effectAt(activeRow));
		effects.removeAt(activeRow);

	}
	if (effects.size()>0 && activeRow>0)
	activeRow--;
	setupListView();
	
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
		//TODO effects.append(result->data().toString());
		foreach (EffectsList* e, effectLists.values()){
			QDomElement dom=e->getEffectByName(result->data().toString());
			clipref->addEffect(dom);
			slotClipItemSelected(clipref);
		}
		
		setupListView();
		//kDebug()<< result->data();
	}
	delete displayMenu;
	
}

void EffectStackView::itemSelectionChanged (){
	//kDebug() << "drop";
}
#include "effectstackview.moc"
