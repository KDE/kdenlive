/***************************************************************************
                          flatbutton.h  -  description
                             -------------------
    begin                : Tue Jan 24 2006
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

#ifndef FLATBUTTON_H
#define FLATBUTTON_H

#include <qstring.h>
#include <qevent.h>
#include <qlabel.h>
#include <qpixmap.h>



/**Describes a "flat" borderless clickable button
  *@author Jean-Baptiste Mardelle
  */
namespace Gui {

class FlatButton:public QLabel {
    Q_OBJECT public:
      FlatButton(QWidget * parent = 0, const char *name = 0, const QPixmap &on_pix = 0, const QPixmap &off_pix = 0, bool isOn = true);
    virtual ~FlatButton();

    virtual void mousePressEvent( QMouseEvent * );
    virtual void enterEvent ( QEvent * );
    virtual void leaveEvent ( QEvent * );
    
  private:
      bool m_isOn;
      QPixmap m_onPixmap;
      QPixmap m_offPixmap;
      
  signals:
      void clicked();
};

};

#endif
