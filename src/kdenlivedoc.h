/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#ifndef KDENLIVEDOC_H
#define KDENLIVEDOC_H

#include <qdom.h>
#include <qstring.h>
#include <qmap.h>
#include <QList>
#include <QObject>

#include <KUndoStack>
#include <kurl.h>

#include "gentime.h"
#include "timecode.h"
#include "renderer.h"
#include "clipmanager.h"

class KdenliveDoc:public QObject {
  Q_OBJECT public:

    KdenliveDoc(const KUrl &url, double fps, int width, int height, QWidget *parent = 0);
    ~KdenliveDoc();
    QString documentName();
    QDomNodeList producersList();
    double fps();
    int width();
    int height();
    KUrl url();
    void setProducers(QDomElement doc);
    Timecode timecode();
    QDomDocument toXml();
    void setRenderer(Render *render);
    KUndoStack *commandStack();
    QString producerName(int id);
    void setProducerDuration(int id, int duration);
    int getProducerDuration(int id);
    Render *renderer();
    ClipManager *clipManager();
    void addClip(const QDomElement &elem, const int clipId);
    void slotAddClipFile(const KUrl url, const QString group);
    void slotAddColorClipFile(const QString name, const QString color, QString duration, const QString group);
    void deleteClip(const uint clipId);
    int getFramePos(QString duration);
    DocClipBase *getBaseClip(int clipId);
    void updateClip(int id);

  private:
    KUrl m_url;
    QDomDocument m_document;
    QString m_projectName;
    double m_fps;
    int m_width;
    int m_height;
    Timecode m_timecode;
    Render *m_render;
    KUndoStack *m_commandStack;
    QDomDocument generateSceneList();
    ClipManager *m_clipManager;

  public slots:
    
  signals:
    void addProjectClip(DocClipBase *);
    void updateClipDisplay(int);
};

#endif
