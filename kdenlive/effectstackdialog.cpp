/***************************************************************************
                          effectstackdialog  -  description
                             -------------------
    begin                : Mon Jan 12 2004
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
#include "effectstackdialog.h"

#include <kdebug.h>
#include <kiconloader.h>
#include <kpushbutton.h>
#include <effectstacklistview.h>

#include "docclipref.h"
#include "effect.h"

EffectStackDialog::EffectStackDialog(QWidget *parent, const char *name )
 : EffectStackDialog_UI(parent, name)
{
	KIconLoader loader;

	m_upButton->setPixmap( loader.loadIcon( "1uparrow", KIcon::Toolbar ) );
	m_downButton->setPixmap( loader.loadIcon( "1downarrow", KIcon::Toolbar ) );
	m_deleteButton->setPixmap( loader.loadIcon( "edit_remove", KIcon::Toolbar ) );
}


EffectStackDialog::~EffectStackDialog()
{
}

void EffectStackDialog::slotSetEffectStack(DocClipRef *clip)
{
	m_effectList->setEffectStack(clip);
}

