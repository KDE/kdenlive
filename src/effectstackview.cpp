#include <KDebug>
#include <KLocale>

#include "effectstackview.h"
#include "clipitem.h"

EffectStackView::EffectStackView( QWidget *parent)
: QWidget(parent)
{
	ui.setupUi(this);
	clipref=NULL;
}

void EffectStackView::slotClipItemSelected(ClipItem* c)
{
	clipref=c;
	if (clipref==NULL)
		return;
	
	ui.effectlist->clear();

	ui.effectlist->addItems(clipref->effectNames());
	
	
}

#include "effectstackview.moc"