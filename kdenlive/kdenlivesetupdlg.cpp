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
#include <kconfig.h>

#include "kdenlivesetupdlg.h"
#include "rendersetupdlg.h"
#include "kdenlive.h"
#include "kdenlivesettings.h"


namespace Gui {

    KdenliveSetupDlg::KdenliveSetupDlg(KdenliveApp * app, QWidget * parent,
	const char *name):KConfigDialog(parent, name,
	KdenliveSettings::self()) {
	page1 = new configInterface();
	page2 = new configTimeline();

	addPage(page1, i18n("Interface"), "looknfeel");
	addPage(page2, i18n("Timeline"), "view");
    } KdenliveSetupDlg::~KdenliveSetupDlg() {
    }




}				// namespace Gui
