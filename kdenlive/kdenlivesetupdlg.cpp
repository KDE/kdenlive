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
 
#include "kdenlivesetupdlg.h"
#include "rendersetupdlg.h"

KdenliveSetupDlg::KdenliveSetupDlg(QWidget *parent, const char *name ) :
                              KDialogBase(IconList,
                                          i18n("Kdenlive Setup"),
                                          Help | Default | Ok | Apply | Cancel,
                                          Ok,
                                          parent, name)
{
   QFrame *page = addPage( i18n("Renderer") );
   QVBoxLayout *topLayout = new QVBoxLayout( page, 0, 6 );
   topLayout->addWidget( new RenderSetupDlg(page, "renderdlg" ));
}

KdenliveSetupDlg::~KdenliveSetupDlg()
{
}
