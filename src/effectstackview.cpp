#include <KDebug>
#include <KLocale>

#include "effectstackview.h"
#include "clipitem.h"

EffectStackView::EffectStackView( QWidget *parent)
: QWidget(parent)
{
	ui.setupUi(this);
	
}

void EffectStackView::slotClipItemSelected(ClipItem*)
{
	kDebug() << "selected" ;
	kDebug() << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
}

#include "effectstackview.moc"