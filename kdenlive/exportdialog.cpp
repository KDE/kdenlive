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
#include "qlayout.h"

ExportDialog::ExportDialog(QPtrList<AVFileFormatDesc> &formatList, QWidget *parent, const char *name ) :
//                        KJanusWidget(parent,name, IconList),
                        KJanusWidget(parent,name, TreeList),
                        m_formatList(formatList)
{
  generateLayout();
}

ExportDialog::~ExportDialog()
{
}

/** Generate a layout for this dialog, based upon the values that have been passed to it. */
void ExportDialog::generateLayout()
{
  QPtrListIterator<AVFileFormatDesc> itt(m_formatList);

  while(itt.current()) {
    QFrame *frame = addPage(itt.current()->name());
    QHBoxLayout *topLayout = new QHBoxLayout( frame, 0, 6 );
    itt.current()->createWidget(frame);
    ++itt;
  }
}
