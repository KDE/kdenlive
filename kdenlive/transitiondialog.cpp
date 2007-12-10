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
// #include <knewstuff/downloaddialog.h>

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
    m_container = new QGridLayout(this, 2, 5);
    m_container->setMargin(5);

    trackLab = new QLabel(i18n("Perform transition with: "), this);
    trackPolicy = new KComboBox(this);
    refreshTracks(app->getDocument()->trackList().count());

    transLab = new QLabel(i18n("Duration: "), this);
    transitionDuration = new KRestrictedLine(this);
    transitionDuration->setInputMask("99:99:99:99; ");

    m_videoFormat = app->projectFormatParameters(app->projectVideoFormat());
    propertiesDialog = new KJanusWidget(this, name, KJanusWidget::IconList);
    transitCrossfade = new transitionCrossfade_UI(propertiesDialog->addVBoxPage(i18n("Crossfade"), QString::null, KGlobal::iconLoader()->loadIcon("kdenlive_trans_down", KIcon::Small, 15)));

    transitWipe = new transitionWipeWidget(propertiesDialog->addVBoxPage(i18n("Push"), QString::null, KGlobal::iconLoader()->loadIcon("kdenlive_trans_wiper", KIcon::Small, 15)));

    transitPip = new transitionPipWidget(app, 200,(int) (200.0 / KdenliveSettings::displayratio()), propertiesDialog->addVBoxPage(i18n("PIP"), QString::null, KGlobal::iconLoader()->loadIcon("kdenlive_trans_pip", KIcon::Small, 15)));

    transitLumaFile = new transitionLumaFile_UI(propertiesDialog->addVBoxPage(i18n("Wipe"), QString::null, KGlobal::iconLoader()->loadIcon("kdenlive_trans_luma", KIcon::Small, 15)));
    transitLumaFile->lumaView->showToolTips();
    transitLumaFile->lumaView->setGridX(60);
    transitLumaFile->lumaView->setGridY(40);

    connect(transitLumaFile->slider_soft ,SIGNAL(valueChanged(int)), transitLumaFile->spin_soft, SLOT(setValue(int)));
    connect(transitLumaFile->spin_soft ,SIGNAL(valueChanged(int)), transitLumaFile->slider_soft, SLOT(setValue(int)));

    /*transitAudiofade = new transitionAudiofade_UI(addPage(i18n("Audio Fade"), QString::null, KGlobal::iconLoader()->loadIcon("kdenlive_trans_down", KIcon::Small, 15)));*/
    initLumaFiles();
    m_container->addWidget(trackLab, 0, 0);
    m_container->addWidget(trackPolicy, 0, 1);
    m_spacer = new QSpacerItem(10, 10, QSizePolicy::MinimumExpanding);
    m_container->addItem(m_spacer, 0, 2);
    m_container->addWidget(transLab, 0, 3);
    m_container->addWidget(transitionDuration, 0, 4);

    m_container->addMultiCellWidget(propertiesDialog, 1, 4, 0, 4);
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
    delete trackLab;
    delete transLab;
    delete trackPolicy;
    delete transitionDuration;
    delete m_container;
}

void TransitionDialog::setVideoFormat(formatTemplate format)
{
    m_videoFormat = format;
    refreshLumas();
}

void TransitionDialog::refreshLumas()
{
    transitWipe->luma_file->clear();
    transitPip->luma_file->clear();
    transitLumaFile->lumaView->clear();
    initLumaFiles();
}

void TransitionDialog::initLumaFiles()
{
    bool foundLumas = false;
    if (m_videoFormat.height() == 480) {
	// NTSC format
	m_lumaType = "lumasNTSC";
	KGlobal::dirs()->addResourceType(m_lumaType, "");
	foundLumas = true;
	QStringList kdenliveLumas = KGlobal::dirs()->findDirs("data", "kdenlive/pgm/NTSC");
	while (!kdenliveLumas.isEmpty()) {
	    KGlobal::dirs()->addResourceDir(m_lumaType, kdenliveLumas.first());
	    kdenliveLumas.pop_front();
	}
        KGlobal::dirs()->addResourceDir(m_lumaType, KdenliveSettings::mltpath() + "/share/mlt/modules/lumas/NTSC");
    }
    else if (m_videoFormat.height() == 1080) {
	// TODO, separate PAL AND NTSC HDV 1080
	m_lumaType = "lumasHDV1080";
	KGlobal::dirs()->addResourceType(m_lumaType, "");

	QStringList kdenliveLumas = KGlobal::dirs()->findDirs("data", "kdenlive/pgm/HDV1080");
	while (!kdenliveLumas.isEmpty()) {
	    foundLumas = true;
	    KGlobal::dirs()->addResourceDir(m_lumaType, kdenliveLumas.first());
	    kdenliveLumas.pop_front();
	}
        KGlobal::dirs()->addResourceDir(m_lumaType, KdenliveSettings::mltpath() + "/share/mlt/modules/lumas/HDV1080");
    }
    else if (m_videoFormat.height() == 720) {
	// TODO, separate PAL AND NTSC HDV 720
	m_lumaType = "lumasHDV720";
	KGlobal::dirs()->addResourceType(m_lumaType, "");

	QStringList kdenliveLumas = KGlobal::dirs()->findDirs("data", "kdenlive/pgm/HDV720");
	while (!kdenliveLumas.isEmpty()) {
	    foundLumas = true;
	    KGlobal::dirs()->addResourceDir(m_lumaType, kdenliveLumas.first());
	    kdenliveLumas.pop_front();
	}
        KGlobal::dirs()->addResourceDir(m_lumaType, KdenliveSettings::mltpath() + "/share/mlt/modules/lumas/HDV720");
    }
    if (!foundLumas) {
	// If no lumas were found default to PAL
	m_lumaType = "lumasPAL";
	KGlobal::dirs()->addResourceType(m_lumaType, "");
	QStringList kdenliveLumas = KGlobal::dirs()->findDirs("data", "kdenlive/pgm/PAL");
	while (!kdenliveLumas.isEmpty()) {
	    KGlobal::dirs()->addResourceDir(m_lumaType, kdenliveLumas.first());
	    kdenliveLumas.pop_front();
	}
        KGlobal::dirs()->addResourceDir(m_lumaType, KdenliveSettings::mltpath() + "/share/mlt/modules/lumas/PAL");
    }

    QStringList iconList = KGlobal::dirs()->KStandardDirs::findAllResources(m_lumaType, "*.pgm");
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
	transitWipe->setFormat(m_lumaType);
	transitPip->luma_file->insertItem(pix, itemName);
    }
}

QMap <QString, QPixmap> TransitionDialog::lumaPreviews()
{
	QMap <QString, QPixmap> lumas;
	for ( int n=0; n<transitWipe->luma_file->count(); n++ )
            lumas[transitWipe->luma_file->text( n )] = *transitWipe->luma_file->pixmap(n);

	return lumas;
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
    //transitPip->radio_start->setChecked(true);
    transitPip->keyframe_number->setValue(0);
}

bool TransitionDialog::checkTransition(DocClipRef *clip)
{
    if (m_transition == NULL || m_transition->belongsToClip(clip)) return true;
    disconnectTransition();
    m_transition = 0;
    resetTransitionDialog();
    setEnabled(false);
    return false;
}

void TransitionDialog::setTransition(Transition *transition, KdenliveDoc *doc)
{
	if (m_transition == transition) return;
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
	QString currentDuration = doc->timeCode().getTimecode(transition->transitionDuration(), doc->framesPerSecond());
    	transitionDuration->setText(currentDuration);

        connectTransition();
}

void TransitionDialog::slotRedrawTransition()
{
    if (!m_transition) return;
    int clipTrack = m_transition->referencedClip()->trackNum();
    GenTime start = m_transition->referencedClip()->trackStart();
    GenTime end = m_transition->referencedClip()->trackEnd();
    emit redrawTransition(clipTrack, start, end);

}

void TransitionDialog::slotMoveCursor(int position)
{
    if (!m_transition) return;
    const GenTime pos = m_transition->referencedClip()->trackStart() + GenTime(position, KdenliveSettings::defaultfps());
    emit adjustCursorPosition(pos);
    
}

void TransitionDialog::connectTransition()
{
   connect(propertiesDialog, SIGNAL( aboutToShowPage ( QWidget * )), this, SLOT(applyChanges()));
   connect(transitionDuration,SIGNAL(textChanged(const QString &)), this, SIGNAL(resizeTransition(const QString &)));
   connect(transitWipe, SIGNAL(applyChanges ()), this, SLOT(applyChanges()));
   connect(transitWipe->use_luma, SIGNAL(toggled(bool)), this, SLOT(applyChanges()));
   connect(transitCrossfade->invertTransition, SIGNAL(released()), this, SLOT(applyChanges()));
   connect(trackPolicy, SIGNAL(activated(int)), this, SLOT(applyChanges()));

   connect(transitPip, SIGNAL(transitionChanged()), this, SLOT(applyChanges()));
   connect(transitPip, SIGNAL(transitionNeedsRedraw()), this, SLOT(slotRedrawTransition()));
   connect(transitPip, SIGNAL(moveCursorToKeyFrame(int)), this, SLOT(slotMoveCursor(int)));
   connect(transitPip->use_luma, SIGNAL(toggled(bool)), this, SLOT(applyChanges()));

   connect(transitLumaFile->spin_soft, SIGNAL(valueChanged(int)), this, SLOT(applyChanges()));
   connect(transitLumaFile->lumaView, SIGNAL(selectionChanged ()), this, SLOT(applyChanges()));
   connect(transitLumaFile->invertTransition, SIGNAL(released()), this, SLOT(applyChanges()));
}

void TransitionDialog::disconnectTransition()
{
    disconnect(propertiesDialog, SIGNAL( aboutToShowPage ( QWidget * )), this, SLOT(applyChanges()));
    disconnect(transitionDuration,SIGNAL(textChanged(const QString &)), this, SIGNAL(resizeTransition(const QString &)));
    disconnect(transitWipe, SIGNAL(applyChanges ()), this, SLOT(applyChanges()));
    disconnect(transitWipe->use_luma, SIGNAL(toggled(bool)), this, SLOT(applyChanges()));
    disconnect(transitCrossfade->invertTransition, SIGNAL(released()), this, SLOT(applyChanges()));
    disconnect(trackPolicy, SIGNAL(activated(int)), this, SLOT(applyChanges()));

    disconnect(transitPip, SIGNAL(transitionChanged()), this, SLOT(applyChanges()));
    disconnect(transitPip, SIGNAL(transitionNeedsRedraw()), this, SLOT(slotRedrawTransition()));
    disconnect(transitPip, SIGNAL(moveCursorToKeyFrame(int)), this, SLOT(slotMoveCursor(int)));
    disconnect(transitPip->use_luma, SIGNAL(toggled(bool)), this, SLOT(applyChanges()));

    disconnect(transitLumaFile->spin_soft, SIGNAL(valueChanged(int)), this, SLOT(applyChanges()));
    disconnect(transitLumaFile->lumaView, SIGNAL(selectionChanged ()), this, SLOT(applyChanges()));
    disconnect(transitLumaFile->invertTransition, SIGNAL(released()), this, SLOT(applyChanges()));
    
}

bool TransitionDialog::isActiveTransition(Transition *transition) const
{
	if (transition == m_transition) return true;
	return false;
}

Transition *TransitionDialog::activeTransition() const
{
	return m_transition;
}

bool TransitionDialog::belongsToClip(DocClipRef *clip)
{
	if (m_transition && m_transition->belongsToClip(clip)) return true;
	return false;
}

int TransitionDialog::selectedKeyFramePosition() const
{
	if (!m_transition || m_transition->transitionType() != Transition::PIP_TRANSITION)
	    return -1;
	return transitPip->getKeyFrameIndex(transitPip->keyframe_number->value());
}


void TransitionDialog::setDuration(const QString &dur)
{
	transitionDuration->setText(dur);
}

void TransitionDialog::applyChanges()
{
	if (m_transition) {
		if (m_transition == 0) return;
		m_transition->setTransitionType(selectedTransition());
        	m_transition->setTransitionParameters(transitionParameters());
		m_transition->setTransitionDirection(transitionDirection());
		m_transition->setTransitionTrack(trackPolicy->currentItem());
		slotRedrawTransition();
		kdDebug()<<" / / / / / / / / / / / / TRANSITION CHANGED"<<endl;
		emit transitionChanged(true);
        }
}

void TransitionDialog::refreshTracks(int trackNb)
{
    trackPolicy->clear();
    uint ix = 1;
    trackPolicy->insertItem(i18n("Automatic - Use next video track"));
    trackPolicy->insertItem(i18n("Background track (black)"));
    while (trackNb > 0) {
	trackPolicy->insertItem(i18n("Track %1").arg(ix));
	trackNb--;
	ix++;
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
	// Push transition
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
	else transitWipe->use_luma->setChecked(false);
    }
    else if (propertiesDialog->activePageIndex() == 2) {
	// Pip transition
        transitPip->setParameters(parameters["geometry"], m_transition->transitionDuration().frames(KdenliveSettings::defaultfps()));
	QString fname = parameters["luma"];
	if (!fname.isEmpty()) {
	    transitPip->use_luma->setChecked(true);
	    fname = KURL(fname).filename();
	    fname = fname.left(fname.length() - 4);
	    transitPip->luma_file->setCurrentText(fname);
	    transitPip->spin_soft->setValue(parameters["softness"].toDouble() * 100.0);
	}
	else transitPip->use_luma->setChecked(false);
    }
    else if (propertiesDialog->activePageIndex() == 3) {
	// Luma transition
	transitLumaFile->slider_soft->setValue(parameters["softness"].toDouble() * 100.0);
	QString fileName = KURL(parameters["resource"]).filename();
	if (fileName.isEmpty()) return;
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

QString TransitionDialog::getLumaFilePath(QString fileName)
{
	return locate(m_lumaType, fileName + ".pgm");
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
	QString fname = locate(m_lumaType, transitPip->luma_file->currentText() + ".pgm");
	paramList["luma"] = fname;
	paramList["softness"] = QString::number(((double) transitPip->spin_soft->value()) / 100.0);
      }
    }
    else if (propertiesDialog->activePageIndex() == 3) // luma file
    {
      QString fname = QString::null;
      if (transitLumaFile->lumaView->currentItem())
          fname = locate(m_lumaType, transitLumaFile->lumaView->currentItem()->text() + ".pgm");
      else if (transitLumaFile->lumaView->firstItem()) 
	  fname = locate(m_lumaType, transitLumaFile->lumaView->firstItem()->text() + ".pgm");
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


