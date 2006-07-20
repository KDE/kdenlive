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

#ifndef ChangeClipSpeed_H
#define ChangeClipSpeed_H

#include <qwidget.h>

#include <krestrictedline.h>
#include <kdialogbase.h>
#include <kurl.h>

#include "gentime.h"
#include "changeclipspeed_ui.h"


namespace Gui {
	class KdenliveApp;
/**Displays the a dialog to change a clip speed (for slowmotion)
  *@author Jean-Baptiste Mardelle
  */

    class changeClipSpeed:public changeClipSpeed_UI {
      Q_OBJECT public:
        changeClipSpeed(int speed, QWidget * parent = 0, const char *name = 0);
        virtual ~changeClipSpeed();

	GenTime duration();
	double selectedSpeed();

      private:
	int m_speed;

      private slots:
	void updateDuration();
	void resetSpeed();
    };

}				// namespace Gui
#endif
