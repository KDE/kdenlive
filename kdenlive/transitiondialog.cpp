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
#include <qgrid.h>
#include <qslider.h>
#include <qspinbox.h>
#include <qcheckbox.h>
#include <qradiobutton.h>

#include <kpushbutton.h>
#include <kiconloader.h>
#include <kiconview.h>
#include <klocale.h>
#include <kdebug.h>
#include <kglobal.h>
#include <kstandarddirs.h>

#include "kdenlive.h"
#include "kdenlivedoc.h"
#include "transition.h"
#include "docclipref.h"
#include "transitiondialog.h"


namespace Gui {

    TransitionDialog::TransitionDialog(KdenliveApp * app, QWidget * parent,
                                       const char *name): QWidget(parent, name), m_transition(0)
{
    QFont dialogFont = font();
    dialogFont.setPointSize(dialogFont.pointSize() - 1);
    setFont(dialogFont);
    QGridLayout *m_container = new QGridLayout(this, 2, 2);
    m_container->setMargin(5);

    QLabel *lab = new QLabel(i18n("Perform transition with: "), this);
    trackPolicy = new KComboBox(this);
    trackPolicy->insertItem(i18n("Automatic - Use next video track"));
    trackPolicy->insertItem(i18n("Background track (black)"));

    int trackNb = app->getDocument()->trackList().count();
    uint ix = 1;
    while (trackNb > 0) {
	trackPolicy->insertItem(i18n("Track %1").arg(ix));
	trackNb--;
	ix++;
    }
    propertiesDialog = new KJanusWidget(this, name, KJanusWidget::IconList);
    transitCrossfade = new transitionCrossfade_UI(propertiesDialog->addVBoxPage(i18n("Crossfade"), QString::null, KGlobal::iconLoader()->loadIcon("kdenlive_trans_down", KIcon::Small, 15)));

    transitWipe = new transitionWipeWidget(propertiesDialog->addVBoxPage(i18n("Push"), QString::null, KGlobal::iconLoader()->loadIcon("kdenlive_trans_wiper", KIcon::Small, 15)));

    transitPip = new transitionPipWidget(app, 240,192, propertiesDialog->addVBoxPage(i18n("PIP"), QString::null, KGlobal::iconLoader()->loadIcon("kdenlive_trans_pip", KIcon::Small, 15)));

    transitLumaFile = new transitionLumaFile_UI(propertiesDialog->addVBoxPage(i18n("Wipe"), QString::null, KGlobal::iconLoader()->loadIcon("kdenlive_trans_wiper", KIcon::Small, 15)));
    transitLumaFile->lumaView->showToolTips();
    transitLumaFile->lumaView->setGridX(60);
    transitLumaFile->lumaView->setGridY(40);

    connect(transitLumaFile->slider_soft ,SIGNAL(valueChanged(int)), transitLumaFile->spin_soft, SLOT(setValue(int)));
    connect(transitLumaFile->spin_soft ,SIGNAL(valueChanged(int)), transitLumaFile->slider_soft, SLOT(setValue(int))); 

    /*transitAudiofade = new transitionAudiofade_UI(addPage(i18n("Audio Fade"), QString::null, KGlobal::iconLoader()->loadIcon("kdenlive_trans_down", KIcon::Small, 15)));*/
    initLumaFiles();
    m_container->addWidget(lab, 0, 0);
    m_container->addWidget(trackPolicy, 0, 1);
    m_container->addMultiCellWidget(propertiesDialog, 1, 1, 0, 1);
    setEnabled(false);
    adjustSize();
}

TransitionDialog::~TransitionDialog() 
{
    disconnectTransition();
    delete transitCrossfade;
    delete transitWipe;
    delete transitPip;
    delete propertiesDialog;
}

void TransitionDialog::initLumaFiles()
{
    QStringList iconList = KGlobal::dirs()->KStandardDirs::findAllResources("data", "kdenlive/pgm/*.pgm");

    for ( QStringList::Iterator it = iconList.begin(); it != iconList.end(); ++it ) {
	QString itemName = KURL(*it).fileName();
	QImage im(*it);
	im = im.smoothScale(30,26);
	QPixmap pix(32,28);
	pix.fill(Qt::black);
	bitBlt(&pix, 1, 1, &im, 0, 0, 30, 26);
	itemName = itemName.left(itemName.length() - 4);
	(void) new QIconViewItem( transitLumaFile->lumaView, itemName, pix );
	transitWipe->luma_file->insertItem(pix, itemName);
	transitPip->luma_file->insertItem(pix, itemName);
    }
}

bool TransitionDialog::isOnTrack(int ix)
{
    if (m_transition && m_transition->transitionDocumentTrack() == ix) return true;
    return false;
}

void TransitionDialog::resetTransitionDialog()
{
    transitCrossfade->invertTransition->setChecked(false);
    transitWipe->resetTransition();
    transitPip->slider_transparency->setValue(0);
    transitPip->slider_size->setValue(100);
    transitPip->slider_x->setValue(0);
    transitPip->slider_y->setValue(0);
    transitPip->radio_start->setChecked(true);
}

void TransitionDialog::setTransition(Transition *transition)
{
        disconnectTransition();
	m_transition = transition;
        if (transition == 0) {
	    resetTransitionDialog();
            setEnabled(false);
            return;
        }
        setEnabled(true);
	setActivePage(transition->transitionType());
        setTransitionDirection(transition->invertTransition());
        setTransitionParameters(transition->transitionParameters());
	trackPolicy->setCurrentItem(m_transition->transitionTrack());
        connectTransition();
}


void TransitionDialog::connectTransition()
{
   connect(propertiesDialog, SIGNAL( aboutToShowPage ( QWidget * )), this, SLOT(applyChanges()));
   connect(transitWipe, SIGNAL(applyChanges ()), this, SLOT(applyChanges()));
   connect(transitWipe->use_luma, SIGNAL(toggled(bool)), this, SLOT(applyChanges()));
   connect(transitCrossfade->invertTransition, SIGNAL(released()), this, SLOT(applyChanges()));
   connect(trackPolicy, SIGNAL(activated(int)), this, SLOT(applyChanges()));
   connect(transitPip, SIGNAL(transitionChanged()), this, SLOT(applyChanges()));
   connect(transitPip->use_luma, SIGNAL(toggled(bool)), this, SLOT(applyChanges()));
   connect(transitLumaFile->spin_soft, SIGNAL(valueChanged(int)), this, SLOT(applyChanges()));
   connect(transitLumaFile->lumaView, SIGNAL(selectionChanged ()), this, SLOT(applyChanges()));
   connect(transitLumaFile->invertTransition, SIGNAL(released()), this, SLOT(applyChanges()));
}

void TransitionDialog::disconnectTransition()
{
    disconnect(propertiesDialog, SIGNAL( aboutToShowPage ( QWidget * )), this, SLOT(applyChanges()));

    disconnect(transitWipe, SIGNAL(applyChanges ()), this, SLOT(applyChanges()));
    disconnect(transitWipe->use_luma, SIGNAL(toggled(bool)), this, SLOT(applyChanges()));
    disconnect(transitCrossfade->invertTransition, SIGNAL(released()), this, SLOT(applyChanges()));
    disconnect(trackPolicy, SIGNAL(activated(int)), this, SLOT(applyChanges()));
    disconnect(transitPip, SIGNAL(transitionChanged()), this, SLOT(applyChanges()));
    disconnect(transitPip->use_luma, SIGNAL(toggled(bool)), this, SLOT(applyChanges()));
    disconnect(transitLumaFile->spin_soft, SIGNAL(valueChanged(int)), this, SLOT(applyChanges()));
    disconnect(transitLumaFile->lumaView, SIGNAL(selectionChanged ()), this, SLOT(applyChanges()));
    disconnect(transitLumaFile->invertTransition, SIGNAL(released()), this, SLOT(applyChanges()));
    
}

bool TransitionDialog::isActiveTransition(Transition *transition)
{
	if (transition == m_transition) return true;
	return false;
}

bool TransitionDialog::belongsToClip(DocClipRef *clip)
{
	if (m_transition && m_transition->belongsToClip(clip)) return true;
	return false;
}


void TransitionDialog::applyChanges()
{
	if (m_transition) {
		if (m_transition == 0) return;
		m_transition->setTransitionType(selectedTransition());
        	m_transition->setTransitionParameters(transitionParameters());
		m_transition->setTransitionDirection(transitionDirection());
		m_transition->setTransitionTrack(trackPolicy->currentItem());
		emit transitionChanged(true);
        }
}

void TransitionDialog::setActivePage(const Transition::TRANSITIONTYPE &pageName)
{
    switch (pageName) {
    case Transition::COMPOSITE_TRANSITION:
	propertiesDialog->showPage(1);
	break;
    case Transition::PIP_TRANSITION:
	propertiesDialog->showPage(2);
	break;
    case Transition::LUMAFILE_TRANSITION:
	propertiesDialog->showPage(3);
	break;
    case Transition::MIX_TRANSITION:
	propertiesDialog->showPage(4);
	break;
    default:
	propertiesDialog->showPage(0);
	break;
    }
}

Transition::TRANSITIONTYPE TransitionDialog::selectedTransition() 
{
    switch (propertiesDialog->activePageIndex()) {
	case 1: 
	return Transition::COMPOSITE_TRANSITION;
	break;
	case 2: 
	return Transition::PIP_TRANSITION;
	break;
	case 3: 
	return Transition::LUMAFILE_TRANSITION;
	break;
	case 4: 
	return Transition::MIX_TRANSITION;
	break;
	default: 
	return Transition::LUMA_TRANSITION;
	break;	
    }
}

void TransitionDialog::setTransitionDirection(bool direc)
{
    if (propertiesDialog->activePageIndex() == 0)
	transitCrossfade->invertTransition->setChecked(direc);
    else if (propertiesDialog->activePageIndex() == 1)
	transitWipe->invertTransition->setChecked(direc);
    else if (propertiesDialog->activePageIndex() == 3)
	transitLumaFile->invertTransition->setChecked(direc);
    //transitAudiofade->invertTransition->setChecked(direc);

}

void TransitionDialog::setTransitionParameters(const QMap < QString, QString > parameters)
{
    if (propertiesDialog->activePageIndex() == 1) {
        transitWipe->rescaleImages->setChecked(parameters["distort"].toInt());
	transitWipe->setParameters(parameters["geometry"]);
	QString fname = parameters["luma"];
	if (!fname.isEmpty()) {
	    transitWipe->use_luma->setChecked(true);
	    fname = KURL(fname).filename();
	    fname = fname.left(fname.length() - 4);
	    transitWipe->luma_file->setCurrentText(fname);
	    transitWipe->spin_soft->setValue(parameters["softness"].toDouble() * 100.0);
	}
    }
    else if (propertiesDialog->activePageIndex() == 2) {
        transitPip->setParameters(parameters["geometry"]);
	QString fname = parameters["luma"];
	if (!fname.isEmpty()) {
	    transitPip->use_luma->setChecked(true);
	    fname = KURL(fname).filename();
	    fname = fname.left(fname.length() - 4);
	    transitPip->luma_file->setCurrentText(fname);
	    transitPip->spin_soft->setValue(parameters["softness"].toDouble() * 100.0);
	}
        }
    else if (propertiesDialog->activePageIndex() == 3) {
	transitLumaFile->slider_soft->setValue(parameters["softness"].toDouble() * 100.0);
	QString fileName = KURL(parameters["resource"]).filename();
	fileName = fileName.left(fileName.length() - 4);
	QIconViewItem *it = transitLumaFile->lumaView->findItem(fileName);
	if (it) {
	    transitLumaFile->lumaView->setSelected(it, true);
	    transitLumaFile->lumaView->ensureItemVisible(it);
	}
	}
}

bool TransitionDialog::transitionDirection()
{
    bool result = false;
    if (propertiesDialog->activePageIndex() == 0) result = transitCrossfade->invertTransition->isChecked();
    else if (propertiesDialog->activePageIndex() == 1) result = transitWipe->invertTransition->isChecked();
    else if (propertiesDialog->activePageIndex() == 3) result = transitLumaFile->invertTransition->isChecked();
    //else if (activePageIndex() == 3) result = transitAudiofade->invertTransition->isChecked();
    //if (activePageIndex() == 2) result = transitPip->invertTransition->isChecked();
    return result;
}

        
const QMap < QString, QString > TransitionDialog::transitionParameters() 
{
    QMap < QString, QString > paramList;
    if (propertiesDialog->activePageIndex() == 0) return paramList; // crossfade
    if (propertiesDialog->activePageIndex() == 1) // wipe
    {
	return transitWipe->parameters();
    }
    else if (propertiesDialog->activePageIndex() == 2) // pip
    {
      paramList["geometry"] = transitPip->parameters();
      paramList["progressive"] = "1";
      if (transitPip->use_luma->isChecked()) {
	QString fname = transitPip->luma_file->currentText();
	fname = locate("data", "kdenlive/pgm/" + fname + ".pgm");
	paramList["luma"] = fname;
	paramList["softness"] = QString::number(((double) transitPip->spin_soft->value()) / 100.0);
      }
    }
    else if (propertiesDialog->activePageIndex() == 3) // luma file
    {
      QString fname;
      if (transitLumaFile->lumaView->currentItem())
          fname = locate("data", "kdenlive/pgm/" + transitLumaFile->lumaView->currentItem()->text() + ".pgm");
      else fname = locate("data", "kdenlive/pgm/" + transitLumaFile->lumaView->firstItem()->text() + ".pgm");
      paramList["resource"] = fname;
      paramList["softness"] = QString::number(((double) transitLumaFile->spin_soft->value()) / 100.0);
    }
    else if (propertiesDialog->activePageIndex() == 4) // audio mix
    {
      paramList["start"] = "0";
      paramList["end"] = "1";
      paramList["combine"] = "1";
    }
    
    return paramList;
}

} // namespace Gui


