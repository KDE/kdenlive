/***************************************************************************
                          clipproperties.h  -  description
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

#ifndef ClipProperties_H
#define ClipProperties_H

#include <qwidget.h>
#include <qlabel.h>
#include <qvbox.h>
#include <qmap.h>

#include <krestrictedline.h>
#include <kdialogbase.h>
#include <kurl.h>

#include "gentime.h"
#include "timecode.h"
#include "kdenlivedoc.h"

#include "clipproperties_ui.h"


class DocClipRef;
class DocClipAVFile;
class KdenliveDoc;

namespace Gui {
    class KdenliveApp;

/**Displays the properties of a sepcified clip.
  *@author Jean-Baptiste Mardelle
  */

    class ClipProperties:public KDialogBase {
      Q_OBJECT public:
              ClipProperties(DocClipRef *refClip, KdenliveDoc * document, QWidget * parent = 0, const char *name = 0);
        virtual ~ ClipProperties();

        QString color();
        QString name();
        QString description();
        GenTime duration();
        QString url();
        bool transparency();
	int ttl();
	QString extension();

      private slots:
        void updateColor(const QColor &c);
        QString formattedSize(uint fileSize);
        void updateThumb(const QString &path);
	void updateList();
	void updateDuration();
        
      private:
        int m_height;
        int m_width;
	int m_imageCount;
        QPixmap *m_pix;
        clipProperties_UI *clipChoice;
        KdenliveDoc *m_document;
    };

}				// namespace Gui
#endif
