/***************************************************************************
                          transitiondialog.cpp  -  description
                             -------------------
    begin                :  Mar 2006
    copyright            : (C) 2006 by Jean-Baptiste Mardelle
    email                : jb@ader.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include <cmath>

#include <klocale.h>

#include <qnamespace.h>
#include <qhgroupbox.h>
#include <qhbox.h>
#include <qlistbox.h>
#include <qlayout.h>

#include "transition.h"
#include "docclipref.h"
#include "transitiondialog.h"


namespace Gui {

    TransitionDialog::TransitionDialog(QWidget * parent,
	const char *name):KDialogBase(parent, name, true,
                        i18n("Transition Dialog"),
                        KDialogBase::Ok | KDialogBase::Cancel) {
    QWidget *qwidgetPage = new QWidget(this);

  // Create the layout for the page
  QVBoxLayout *qvboxlayoutPage = new QVBoxLayout(qwidgetPage);

    transitDialog = new transitionDialog_UI(qwidgetPage);
    (void) new QListBoxText(transitDialog->transitionList, "luma");
    (void) new QListBoxText(transitDialog->transitionList, "composite");
    transitDialog->transitionList->setCurrentItem(0);
    qvboxlayoutPage->addWidget(transitDialog);
    setMainWidget(qwidgetPage);
    }

TransitionDialog::~TransitionDialog() {}

QString TransitionDialog::selectedTransition() 
{
    return transitDialog->transitionList->currentText();
}

}				// namespace Gui
