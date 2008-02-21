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
	ui.buttonNew->setToolTip(i18n("Add new effect"));
	ui.buttonUp->setIcon(KIcon("go-up"));
	ui.buttonUp->setToolTip(i18n("Move effect up"));
	ui.buttonDown->setIcon(KIcon("go-down"));
	ui.buttonDown->setToolTip(i18n("Move effect down"));
	ui.buttonDel->setIcon(KIcon("trash-empty"));
	ui.buttonDel->setToolTip(i18n("Delete effect"));
	
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
	//connect (ui.effectlist, SIGNAL (itemSelectionChanged() ) , this, SLOT ( itemSelectionChanged()));
	connect( this,SIGNAL (transferParamDesc(const QDomElement&,int ,int) ), ui.kplotwidget, SLOT(setPointLists(const QDomElement&,int ,int) ));
	connect(ui.kplotwidget, SIGNAL (parameterChanged(QDomElement ) ), this , SLOT (slotUpdateEffectParams(QDomElement)));
	effectLists["audio"]=audioEffectList;
	effectLists["video"]=videoEffectList;
	effectLists["custom"]=customEffectList;
	
	ui.infoBox->hide();	
	updateButtonStatus();
	
}

void EffectStackView::slotUpdateEffectParams(QDomElement e){
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
		/*QString outstr;
		QTextStream str(&outstr);
		clipref->effectAt(i).save(str,2);
		kDebug() << outstr;*/
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
		ui.parameterList->clear();
		ui.parameterList->addItem("all");
		QDomNodeList namenode = effects.at(activeRow).elementsByTagName("parameter");
		for (int i=0;i<namenode.count();i++){
			QDomNode pa=namenode.item(i);
			QDomNode na=pa.firstChildElement("name");
			ui.parameterList->addItem(na.toElement().text() );
		}
		
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
	ui.kplotwidget->replot(text);
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
