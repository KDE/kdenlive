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

#include <qnamespace.h>
#include <qhgroupbox.h>
#include <qhbox.h>
#include <qlistbox.h>
#include <qlayout.h>
#include <qslider.h>
#include <qcheckbox.h>

#include <kpushbutton.h>
#include <klocale.h>
#include <kdebug.h>

#include "transition.h"
#include "docclipref.h"
#include "transitiondialog.h"


namespace Gui {

    TransitionDialog::TransitionDialog(int width, int height, QWidget * parent,
                                       const char *name):  KDialogBase (KDialogBase::IconList, 0, parent,name, true, i18n("Transition Dialog"), KDialogBase::Ok | KDialogBase::Cancel), m_height(height), m_width(width)
            //KDialogBase(parent, name, true, i18n("Transition Dialog"),KDialogBase::Ok | KDialogBase::Cancel) 
                                       {
    QVBox *page1 = addVBoxPage( i18n("Crossfade") );
    transitCrossfade = new transitionCrossfade_UI(page1);

    QVBox *page2 = addVBoxPage( i18n("Wipe") );
    transitWipe = new transitionWipe_UI(page2);
    
    adjustSize();
    
/*    QWidget *qwidgetPage = new QWidget(this);

  // Create the layout for the page
  QVBoxLayout *qvboxlayoutPage = new QVBoxLayout(qwidgetPage);

    transitDialog = new transitionDialog_UI(qwidgetPage);
    (void) new QListBoxText(transitDialog->transitionList, "luma");
    (void) new QListBoxText(transitDialog->transitionList, "composite");
    transitDialog->transitionList->setCurrentItem(0);
    qvboxlayoutPage->addWidget(transitDialog);
    setMainWidget(qwidgetPage);*/
    }

TransitionDialog::~TransitionDialog() {}

void TransitionDialog::setActivePage(const QString &pageName)
{
    if (pageName == "composite") showPage(1);
}

QString TransitionDialog::selectedTransition() 
{
    QString pageName = "luma";
    if (activePageIndex() == 1) pageName = "composite";
    return pageName;
}

void TransitionDialog::setTransitionDirection(bool direc)
{
    transitCrossfade->invertTransition->setChecked(direc);
    transitWipe->invertTransition->setChecked(direc);
}

void TransitionDialog::setTransitionParameters(const QMap < QString, QString > parameters)
{
    if (activePageIndex() == 1) {
        // parse the "geometry" argument of MLT's composite transition
        transitWipe->rescaleImages->setChecked(parameters["distort"].toInt());
        QString geom = parameters["geometry"];
        QString geom2 = geom.left(geom.find(";"));
        
        if (geom2.right(4).contains(":",FALSE)!=0) { // Start transparency setting
            int pos = geom2.findRev(":");
            QString last = geom2.right(geom2.length() - pos -1);
            transitWipe->transpStart->setValue(100 - last.toInt());
        }
        
        if (geom.right(4).contains(":",FALSE)!=0) { // Ending transparency setting
            int pos = geom.findRev(":");
            QString last = geom.right(geom.length() - pos -1);
            transitWipe->transpEnd->setValue(100 - last.toInt());
            geom.truncate(pos);
        }
        // Which way does it go ?
        if (geom.endsWith("0%,100%:100%x100%")) transitWipe->transitionDown->setOn(true);
        else if (geom.endsWith("0%,-100%:100%x100%")) transitWipe->transitionUp->setOn(true);
        else if (geom.endsWith("-100%,0%:100%x100%")) transitWipe->transitionLeft->setOn(true);
        else if (geom.endsWith("100%,0%:100%x100%")) transitWipe->transitionRight->setOn(true);

    }
}

bool TransitionDialog::transitionDirection()
{
    bool result = true;
    if (activePageIndex() == 0) result = transitCrossfade->invertTransition->isChecked();
    if (activePageIndex() == 1) result = transitWipe->invertTransition->isChecked();
    return result;
}

        
const QMap < QString, QString > TransitionDialog::transitionParameters() 
{
    QMap < QString, QString > paramList;
    if (activePageIndex() == 0) return paramList; // crossfade
    if (activePageIndex() == 1) // wipe
    {
        QString startTransparency = QString::null;
        QString endTransparency = QString::null;
        int transp1 = transitWipe->transpStart->value();
        int transp2 = transitWipe->transpEnd->value();
        if (transp1 > 0) startTransparency = ":" + QString::number(100 - transp1);
        if (transp2 > 0) endTransparency = ":" + QString::number(100 - transp2);
        if (transitWipe->transitionDown->isOn()) paramList["geometry"] = "0=0%,0%:100%x100%" + startTransparency + ";-1=0%,100%:100%x100%" + endTransparency;
        else if (transitWipe->transitionUp->isOn()) paramList["geometry"] = "0=0%,0%:100%x100%" + startTransparency + ";-1=0%,-100%:100%x100%" + endTransparency;
        //else if (transitWipe->transitionRight->isOn()) paramList["geometry"] = "0=0%,0%:100%x100%" + startTransparency + ";-1=100%,0%:100%x100%" + endTransparency;
        else if (transitWipe->transitionRight->isOn()) paramList["geometry"] = "0=0,0:720x576" + startTransparency + ";-1=720,0:720x576" + endTransparency;
        else if (transitWipe->transitionLeft->isOn()) paramList["geometry"] = "0=0%,0%:100%x100%" + startTransparency + ";-1=-100%,0%:100%x100%" + endTransparency;
        paramList["progressive"] = "1";
        if (transitWipe->rescaleImages->isChecked()) paramList["distort"] = "1";
    }
    
}

} // namespace Gui


