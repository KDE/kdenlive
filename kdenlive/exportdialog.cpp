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

#include <klocale.h>
 
#include "exportdialog.h"
#include "qlayout.h"
#include "qobjectlist.h"

ExportDialog::ExportDialog(QPtrList<AVFileFormatDesc> &formatList, QWidget *parent, const char *name ) :
//                        KJanusWidget(parent,name, IconList),
                        KDialogBase(TreeList, i18n("Render Dialog"), Ok | Default | Cancel, Ok, parent, name),
                        m_formatList(formatList)
{
  m_pageList.setAutoDelete(true);
  generateLayout();
}

ExportDialog::~ExportDialog()
{
}

/** Generate a layout for this dialog, based upon the values that have been passed to it. */
void ExportDialog::generateLayout()
{
  m_pageList.clear();
  
  QPtrListIterator<AVFileFormatDesc> itt(m_formatList);

  while(itt.current()) {
    QFrame *frame = addPage(itt.current()->name());
    m_pageList.append(frame);    
    QHBoxLayout *layout = new QHBoxLayout( frame, 0, 6 );
    QWidget *widget = itt.current()->createWidget(frame);
    layout->addWidget(widget);
    ++itt;
  }
}

/** Specify a new file format list, and reconstruct the dialog box. */
void ExportDialog::setFormatList(const QPtrList<AVFileFormatDesc> &list)
{
  m_formatList = list;
  generateLayout();
}

/** Returns the url set inside of this export dialog. */
KURL ExportDialog::url()
{
  return KURL("/tmp/test.dv");
}
