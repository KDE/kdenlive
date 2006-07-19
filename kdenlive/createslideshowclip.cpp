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

#include <kurlrequester.h>
#include <klocale.h>
#include <kdebug.h>

#include "createslideshowclip.h"


namespace Gui {

    createSlideshowClip::createSlideshowClip(QWidget * parent, const char *name):  KDialogBase (KDialogBase::Swallow, 0, parent,name, true, i18n("Create Slideshow"), KDialogBase::Ok | KDialogBase::Cancel), m_extension("png") {

	clipChoice = new createSlideshowClip_UI(this);
	clipChoice->url_image->setMode(KFile::Directory);
	setMainWidget(clipChoice);
	clipChoice->adjustSize();
	clipChoice->show();
    }

    createSlideshowClip::~createSlideshowClip()
    {}

    QString createSlideshowClip::selectedFolder()
    {
	return clipChoice->url_image->url();
    }

    QString createSlideshowClip::selectedExtension()
    {
	return m_extension;
    }

    int createSlideshowClip::ttl()
    {
	return clipChoice->image_ttl->value();
    }

    QString createSlideshowClip::description()
    {
	return clipChoice->edit_description->text();
    }

    bool createSlideshowClip::isTransparent()
    {
	return clipChoice->transparentBg->isChecked();
    }

} // namespace Gui


