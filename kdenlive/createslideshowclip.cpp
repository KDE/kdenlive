/***************************************************************************
                          createslideshowclip.cpp  -  description
                             -------------------
    begin                :  Jul 2006
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

#include <qlistbox.h>
#include <qslider.h>
#include <qspinbox.h>
#include <qcheckbox.h>
#include <qstringlist.h>
#include <qdir.h>
#include <qlabel.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qradiobutton.h>

#include <kcombobox.h>
#include <kurlrequester.h>
#include <klocale.h>
#include <kdebug.h>


#include "timecode.h"
#include "kdenlivesettings.h"
#include "createslideshowclip.h"
#include "kdenlivedoc.h"

namespace Gui {

    createSlideshowClip::createSlideshowClip(KdenliveApp * parent, const char *name):  KDialogBase (KDialogBase::Swallow, 0, parent,name, true, i18n("Create Slideshow"), KDialogBase::Ok | KDialogBase::Cancel), m_app(parent), m_imageCount(0) {

	clipChoice = new createSlideshowClip_UI(this);
	clipChoice->url_image->setMode(KFile::Directory);
	setMainWidget(clipChoice);
	clipChoice->adjustSize();
	clipChoice->show();
	// default slideshow duration
	clipChoice->image_ttl->setText("00:00:02:00");
	clipChoice->transition_ttl->setText("00:00:00:10");
	enableButtonOK(false);
	connect(clipChoice->url_image, SIGNAL(textChanged (const QString &)), this, SLOT(updateList()));
	connect(clipChoice->imageType, SIGNAL(activated (int)), this, SLOT(updateList()));
	connect(clipChoice->image_ttl, SIGNAL(textChanged(const QString &)), this, SLOT(updateDuration()));
    }

    createSlideshowClip::~createSlideshowClip()
    {}

    QString createSlideshowClip::selectedFolder() const
    {
	return clipChoice->url_image->url();
    }

    QString createSlideshowClip::selectedExtension() const
    {
	return clipChoice->imageType->currentText();
    }

    QString createSlideshowClip::currentLuma() const
    {
	return clipChoice->luma_list->currentText();
    }

    bool createSlideshowClip::useLuma() const
    {
	return clipChoice->luma_selected->isChecked();
    }

    void createSlideshowClip::insertLuma(const QPixmap &pix, const QString &txt)
    {
	clipChoice->luma_list->insertItem(pix, txt);
    }

    double createSlideshowClip::softness() const
    {
	return clipChoice->luma_softness->value() /100.0;
    }

    QString createSlideshowClip::ttl() const
    {
	return clipChoice->image_ttl->text();
    }

    int createSlideshowClip::imageCount() const
    {
	return m_imageCount;
    }

    QString createSlideshowClip::lumaDuration() const
    {
	return clipChoice->transition_ttl->text();
    }

    QString createSlideshowClip::duration() const
    {
	return clipChoice->edit_duration->text();
    }

    QString createSlideshowClip::description() const
    {
	return clipChoice->edit_description->text();
    }

    bool createSlideshowClip::isTransparent() const
    {
	return clipChoice->transparentBg->isChecked();
    }

    bool createSlideshowClip::hasCrossfade() const
    {
	return clipChoice->crossfade->isChecked();
    }

    void createSlideshowClip::updateDuration()
    {
	double fps = KdenliveSettings::defaultfps();
	int dur = m_app->getDocument()->getTimecodePosition(ttl()).frames(fps) * m_imageCount;
        clipChoice->edit_duration->setText(m_app->getDocument()->timeCode().getTimecode(GenTime(dur, fps), fps));
    }

    void createSlideshowClip::updateList()
    {
	QStringList more;
    	QStringList::Iterator it;

        QDir dir( selectedFolder() );
        more = dir.entryList( QDir::Files );
	m_imageCount = 0;
	clipChoice->imageList->clear();

        for ( it = more.begin() ; it != more.end() ; ++it )
            if ((*it).endsWith("." + selectedExtension(), FALSE)) {
		m_imageCount++;
		QPixmap p = QImage(selectedFolder() + "/" + (*it)).smoothScale(50, 40);
		clipChoice->imageList->insertItem(p, (*it));
	    }

	updateDuration();

	clipChoice->scanResult->setText(i18n("Found %1 images in selected folder").arg(QString::number(m_imageCount)));

	if (m_imageCount == 0) enableButtonOK(false);
	else enableButtonOK(true);
    }

} // namespace Gui


