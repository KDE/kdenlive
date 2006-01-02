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

namespace Gui
{

EffectStackDialog::EffectStackDialog(KdenliveApp *app, KdenliveDoc *doc, QWidget *parent, const char *name )
 : EffectStackDialog_UI(parent, name)
{
	KIconLoader loader;

	m_upButton->setIconSet( QIconSet( loader.loadIcon( "1uparrow", KIcon::Toolbar ) ));
	m_downButton->setIconSet( QIconSet( loader.loadIcon( "1downarrow", KIcon::Toolbar ) ));
	m_deleteButton->setIconSet( QIconSet( loader.loadIcon( "editdelete", KIcon::Toolbar ) ));

	// HACK - We are setting app and doc here because we cannot pass app and doc directly via the auto-generated UI file. This
	// needs to be fixed...
	m_effectList->setAppAndDoc(app, doc);

	connect(m_upButton, SIGNAL(clicked()), m_effectList, SLOT(slotMoveEffectUp()));
	connect(m_downButton, SIGNAL(clicked()), m_effectList, SLOT(slotMoveEffectDown()));
	connect(m_deleteButton, SIGNAL(clicked()), m_effectList, SLOT(slotDeleteEffect()));
	connect(m_effectList, SIGNAL(effectSelected(DocClipRef *, Effect *)), this, SIGNAL(effectSelected(DocClipRef *, Effect *)));
}


EffectStackDialog::~EffectStackDialog()
{
}

void EffectStackDialog::slotSetEffectStack(DocClipRef *clip)
{
	m_effectList->setEffectStack(clip);
}

} // namespace Gui
