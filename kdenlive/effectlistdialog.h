/***************************************************************************
                          effectlistdialog.h  -  description
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

#ifndef EFFECTLISTDIALOG_H
#define EFFECTLISTDIALOG_H

#include <qwidget.h>
#include <klistview.h>
#include <qptrlist.h>

#include "effectdesc.h"

/**The effectList dialog displays a list of effects and transitions known by the renderer. Effects in this dialog can be dragged to a clip, or in the case of transitions, dragged to two or more clips (how is yet to be determined)
  *@author Jason Wood
  */

class EffectListDialog : public KListView  {
   Q_OBJECT
public: 
	EffectListDialog(const QPtrList<EffectDesc> &effectList, QWidget *parent=0, const char *name=0);
	~EffectListDialog();
private:  
  /** Generates the layout for this widget. */
  void generateLayout(const QPtrList<EffectDesc> &effectList);  
public slots:
  /** Set the effect list displayed by this dialog. */
  void setEffectList(const QPtrList<EffectDesc> &effectList);
};

#endif
