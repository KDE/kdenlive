/***************************************************************************
                        effectstacklistview  -  description
                           -------------------
  begin                : Sun Feb 22 2004
  copyright            : (C) 2004 by Jason Wood
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
#include "effectstacklistview.h"

#include <kdebug.h>
#include <klocale.h>

EffectStackListView::EffectStackListView( QWidget * parent, const char * name ) :
		KListView( parent, name )
{
	addColumn(i18n("Effect Stack"));
	setSorting(-1);
	setColumnWidthMode(0, Maximum);
	setAcceptDrops( true );
	setFullWidth( true );
	connect( this, SIGNAL( executed( QListViewItem * ) ), this, SLOT( selectedEffect( QListViewItem * ) ) );
}


EffectStackListView::~EffectStackListView()
{}

void EffectStackListView::updateEffectStack()
{
	clear();

	if ( m_clip ) {
		for ( EffectStack::const_iterator itt = m_clip->effectStack().begin(); itt != m_clip->effectStack().end(); ++itt ) {
			insertItem( new QListViewItem( this, ( *itt ) ->name() ) );
		}
	}

	triggerUpdate();
}

void EffectStackListView::setEffectStack( DocClipRef *clip )
{
	m_clip = clip;
	updateEffectStack();
}

void EffectStackListView::selectedEffect( QListViewItem *item )
{
	if ( m_clip ) {
		QListViewItem * itemItt = firstChild();

		EffectStack::iterator itt = m_clip->effectStack().begin();
		while ( ( itemItt ) && ( itt != m_clip->effectStack().end() ) ) {
			if ( item == itemItt ) {
				emit effectSelected( m_clip, *itt );
			}
			++itt;
			itemItt = itemItt->nextSibling();
		}
		if ( itt == m_clip->effectStack().end() ) {
			kdWarning() << "EffectStackDialog::selectedEffect(QListViewItem *item) iitem not found" << endl;
		}
	} else {
		kdWarning() << "EffectStackDialog::selectedEffect() : m_clip is NULL" << endl;
	}
}
