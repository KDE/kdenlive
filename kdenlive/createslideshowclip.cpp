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
#include <qspinbox.h>
#include <qcheckbox.h>
#include <qstringlist.h>
#include <qdir.h>
#include <qlabel.h>
#include <qpixmap.h>
#include <qimage.h>

#include <kcombobox.h>
#include <kurlrequester.h>
#include <klocale.h>
#include <kdebug.h>


#include "timecode.h"
#include "kdenlivesettings.h"
#include "createslideshowclip.h"


namespace Gui {

    createSlideshowClip::createSlideshowClip(QWidget * parent, const char *name):  KDialogBase (KDialogBase::Swallow, 0, parent,name, true, i18n("Create Slideshow"), KDialogBase::Ok | KDialogBase::Cancel) {

	clipChoice = new createSlideshowClip_UI(this);
	clipChoice->url_image->setMode(KFile::Directory);
	setMainWidget(clipChoice);
	clipChoice->adjustSize();
	clipChoice->show();
	enableButtonOK(false);
	connect(clipChoice->url_image, SIGNAL(textChanged (const QString &)), this, SLOT(updateList()));
	connect(clipChoice->imageType, SIGNAL(activated (int)), this, SLOT(updateList()));
	connect(clipChoice->image_ttl, SIGNAL(valueChanged(int)), this, SLOT(updateDuration()));
    }

    createSlideshowClip::~createSlideshowClip()
    {}

    QString createSlideshowClip::selectedFolder()
    {
	return clipChoice->url_image->url();
    }

    QString createSlideshowClip::selectedExtension()
    {
	return clipChoice->imageType->currentText();
    }


    int createSlideshowClip::ttl()
    {
	return clipChoice->image_ttl->value();
    }

    int createSlideshowClip::imageCount()
    {
	return m_imageCount;
    }

    GenTime createSlideshowClip::duration()
    {
        QString d = clipChoice->edit_duration->text();
        int frames = (d.section(":",0,0).toInt()*3600 + d.section(":",1,1).toInt()*60 + d.section(":",2,2).toInt()) * KdenliveSettings::defaultfps() + d.section(":",3,3).toInt();
        return GenTime(frames , KdenliveSettings::defaultfps());
    }

    QString createSlideshowClip::description()
    {
	return clipChoice->edit_description->text();
    }

    bool createSlideshowClip::isTransparent()
    {
	return clipChoice->transparentBg->isChecked();
    }

    bool createSlideshowClip::hasCrossfade()
    {
	return clipChoice->crossfade->isChecked();
    }

    void createSlideshowClip::updateDuration()
    {
	Timecode tcode;
        clipChoice->edit_duration->setText(tcode.getTimecode(GenTime(m_imageCount * ttl(), KdenliveSettings::defaultfps()), KdenliveSettings::defaultfps()));
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


