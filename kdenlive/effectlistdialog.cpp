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

#include "effectlistdialog.h"

#include "krender.h"

EffectListDialog::EffectListDialog(const QPtrList<EffectDesc> &effectList, QWidget *parent, const char *name ) :
                                          KListView(parent,name)
{
  addColumn(i18n("Effect"));
  generateLayout(effectList);

  setDragEnabled(true);
  setFullWidth(true);  
}

EffectListDialog::~EffectListDialog()
{
}

/** Generates the layout for this widget. */
void EffectListDialog::generateLayout(const QPtrList<EffectDesc> &effectList)
{
  clear();
  
  QPtrListIterator<EffectDesc> itt(effectList);
  while(itt.current()) {
    new KListViewItem(this, itt.current()->name());
    ++itt;
  }
}

/** Set the effect list displayed by this dialog. */
void EffectListDialog::setEffectList(const QPtrList<EffectDesc> &effectList)
{
  generateLayout(effectList);
}
