/***************************************************************************
                          createslideshowclip.h  -  description
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

#ifndef SlideshowClip_H
#define SlideshowClip_H

#include <qwidget.h>

#include <krestrictedline.h>
#include <kdialogbase.h>
#include <kurl.h>

#include "createslideshowclip_ui.h"


namespace Gui {
	class KdenliveApp;
/**Displays the a dialog to create a slideshow clip
  *@author Jean-Baptiste Mardelle
  */

    class createSlideshowClip:public KDialogBase {
      Q_OBJECT public:
        createSlideshowClip(QWidget * parent = 0, const char *name = 0);
        virtual ~createSlideshowClip();

	QString selectedFolder();
	QString selectedExtension();
	int ttl();
	QString description();
	bool isTransparent();

      private:
	QString m_extension;
	createSlideshowClip_UI *clipChoice;
    };

}				// namespace Gui
#endif
