/***************************************************************************
                          exportdialog.cpp  -  description
                             -------------------
    begin                : Tue Jan 21 2003
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

#include "exportdialog.h"

#include <qlayout.h>
#include <qobjectlist.h>

#include <klocale.h>

#include "avfileformatwidget.h"
#include "avformatwidgetbase.h"
#include "exportconfig.h"

ExportDialog::ExportDialog(QPtrList < AVFileFormatDesc > &formatList,
    QWidget * parent, const char *name):KDialogBase(Plain,
    i18n("Render Dialog"), Ok | Default | Cancel, Ok, parent, name)
{
    QHBoxLayout *topLayout = new QHBoxLayout(plainPage(), 0, 6);
    m_exportConfig =
	new ExportConfig(formatList, plainPage(), "export config2");
    topLayout->addWidget(m_exportConfig);
}

ExportDialog::~ExportDialog()
{
}

void ExportDialog::setFormatList(const QPtrList < AVFileFormatDesc > &list)
{
    m_exportConfig->setFormatList(list);
}

KURL ExportDialog::url()
{
    return m_exportConfig->url();
}
