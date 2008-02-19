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
