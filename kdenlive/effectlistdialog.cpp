/***************************************************************************
                         effectlistdialog.cpp  -  description
                            -------------------
   begin                : Sun Feb 9 2003
   copyright            : (C) 2003 by Jason Wood
   email                : jasonwood@blueyonder.co.uk
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <kdebug.h>
#include <klocale.h>

#include "effectdrag.h"
#include "effectlistdialog.h"

#include "krender.h"

namespace Gui
{

EffectListDialog::EffectListDialog( const QPtrList<EffectDesc> &effectList, QWidget *parent, const char *name ) :
		KListView( parent, name )
{
	m_effectList.setAutoDelete(false);

	addColumn( i18n( "Effect" ) );
	setEffectList( effectList );

	setDragEnabled( true );
	setFullWidth( true );

	connect(this, SIGNAL(executed(QListViewItem *)), this, SLOT(slotEffectSelected(QListViewItem* )));
}

EffectListDialog::~EffectListDialog()
{}

/** Generates the layout for this widget. */
void EffectListDialog::generateLayout()
{
	clear();

	QPtrListIterator<EffectDesc> itt( m_effectList );
	while ( itt.current() ) {
		new KListViewItem( this, itt.current() ->name() );
		++itt;
	}
}

/** Set the effect list displayed by this dialog. */
void EffectListDialog::setEffectList( const QPtrList<EffectDesc> &effectList )
{
	m_effectList = effectList;
	generateLayout();
}

void EffectListDialog::slotEffectSelected(QListViewItem *item)
{
	QPtrListIterator<EffectDesc> itt( m_effectList );
	while ( itt.current() ) {
		if(itt.current()->name() == item->text(0)) {
			// emit effectSelected(*itt.current());
		}
		++itt;
	}
}

/** returns a drag object which is used for drag operations. */
QDragObject *EffectListDialog::dragObject()
{
	QListViewItem *selected = selectedItem();

	kdWarning() << "Returning appropriate dragObejct" << endl;

	EffectDesc *desc = findDescription(selected->text(0));

	if(!desc) {
		kdWarning() << "no selected item in effect list" << endl;
		return 0;
	}

	Effect *effect = desc->createEffect();
	return new EffectDrag( effect, this, "drag object");
}

EffectDesc *EffectListDialog::findDescription(const QString &name)
{
	EffectDesc *desc = 0;

	QPtrListIterator<EffectDesc> itt(m_effectList);

	while(itt.current()) {
		if(itt.current()->name() == name) {
			desc = itt.current();
			break;
		}
		++itt;
	}

	return desc;
}

} // namespace Gui
