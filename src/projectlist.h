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


#ifndef PRJECTLIST_H
#define PRJECTLIST_H

#include <QDomNodeList>
#include <QToolBar>
#include <QTreeWidget>

#include <KUndoStack>
#include <KTreeWidgetSearchLine>

#include "docclipbase.h"
#include "kdenlivedoc.h"
#include "renderer.h"
#include "timecode.h"

class ProjectItem;

class ProjectList : public QWidget
{
  Q_OBJECT
  
  public:
    ProjectList(QWidget *parent=0);

    QDomElement producersList();
    void setRenderer(Render *projectRender);

    void addClip(const QStringList &name, const QDomElement &elem, const int clipId, const KUrl &url = KUrl(), int parentId = -1);
    void deleteClip(const int clipId);

  public slots:
    void setDocument(KdenliveDoc *doc);
    void addProducer(QDomElement producer, int parentId = -1);
    void slotReplyGetImage(int clipId, int pos, const QPixmap &pix, int w, int h);
    void slotReplyGetFileProperties(int clipId, const QMap < QString, QString > &properties, const QMap < QString, QString > &metadata);


  private:
    QTreeWidget *listView;
    KTreeWidgetSearchLine *searchView;
    Render *m_render;
    Timecode m_timecode;
    double m_fps;
    QToolBar *m_toolbar;
    KUndoStack *m_commandStack;
    int m_clipIdCounter;
    void selectItemById(const int clipId);
    ProjectItem *getItemById(int id);

  private slots:
    void slotAddClip();
    void slotRemoveClip();
    void slotEditClip();
    void slotClipSelected();
    void slotAddColorClip();

  signals:
    void clipSelected(const QDomElement &);
    void getFileProperties(const QDomElement&, int);
};

#endif
