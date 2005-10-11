/***************************************************************************
                          kdenlivesetupdlg.cpp  -  description
                             -------------------
    begin                : Sat Dec 28 2002
    copyright            : (C) 2002 by Jason Wood
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

#include <qlayout.h>
#include <qlabel.h>
#include <klocale.h>
#include <kiconloader.h>

#include "kdenlivesetupdlg.h"
#include "rendersetupdlg.h"
#include "kdenlive.h"

namespace Gui
{

KdenliveSetupDlg::KdenliveSetupDlg(KdenliveApp *app, QWidget *parent, const char *name ) :
                              KDialogBase(IconList,
                                          i18n("Kdenlive Setup"),
                                          Help | Default | Ok | Apply | Cancel,
                                          Ok,
                                          parent, name)
{
  QFrame *page = addPage( i18n("Renderer"),
		          i18n("Setup External Renderer Program"),
			  KGlobal::instance()->iconLoader()->loadIcon( "piave", KIcon::NoGroup, KIcon::SizeMedium ) );

  QVBoxLayout *topLayout = new QVBoxLayout( page, 0, 6 );
  m_renderDlg = new RenderSetupDlg(app->renderManager(), page, "renderdlg" );
  topLayout->addWidget( m_renderDlg );
}

KdenliveSetupDlg::~KdenliveSetupDlg()
{
}

/** Occurs when the apply button is clicked. */
void KdenliveSetupDlg::slotApply()
{
  m_renderDlg->writeSettings();
}

/** Called when the ok button is clicked. */
void KdenliveSetupDlg::slotOk()
{
  m_renderDlg->writeSettings();
  accept();
}

/** Called when the cancel button is clicked. */
void KdenliveSetupDlg::slotCancel()
{
  reject();
}

/** Called when the "Default" button is pressed. */
void KdenliveSetupDlg::slotDefault()
{
  m_renderDlg->readSettings();
}

} // namespace Gui
