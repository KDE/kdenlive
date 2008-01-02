/***************************************************************************
                         krender.h  -  description
                            -------------------
   begin                : Fri Nov 22 2002
   copyright            : (C) 2002 by Jason Wood
   email                : jasonwood@blueyonder.co.uk
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KDENLIVEDOC_H
#define KDENLIVEDOC_H

#include <qdom.h>
#include <qstring.h>
#include <qmap.h>
#include <QList>
#include <QObject>

#include <kurl.h>

#include "gentime.h"
#include "timecode.h"
#include "renderer.h"

class KdenliveDoc:public QObject {
  Q_OBJECT public:

    KdenliveDoc(KUrl url, double fps, int width, int height, QWidget *parent = 0);
    ~KdenliveDoc();
    QString documentName();
    QDomNodeList producersList();
    double fps();
    int width();
    int height();
    void setProducers(QDomElement doc);
    Timecode timecode();
    QDomDocument toXml();
    void setRenderer(Render *render);

  private:
    KUrl m_url;
    QDomDocument m_document;
    QString m_projectName;
    double m_fps;
    int m_width;
    int m_height;
    Timecode m_timecode;
    Render *m_render;
    QDomDocument generateSceneList();

  public slots:
    
};

#endif
