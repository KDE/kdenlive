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

#include "definitions.h"
#include "kdenlivedoc.h"
#include "renderer.h"
#include "timecode.h"
#include "projectlistview.h"

class ProjectItem;

class ProjectList : public QWidget
{
  Q_OBJECT
  
  public:
    ProjectList(QWidget *parent=0);
    virtual ~ProjectList();

    QDomElement producersList();
    void setRenderer(Render *projectRender);

    void addClip(const QStringList &name, const QDomElement &elem, const int clipId, const KUrl &url = KUrl(), const QString &group = QString::null, int parentId = -1);
    void deleteClip(const int clipId);

  public slots:
    void setDocument(KdenliveDoc *doc);
    void addProducer(QDomElement producer, int parentId = -1);
    void slotReplyGetImage(int clipId, int pos, const QPixmap &pix, int w, int h);
    void slotReplyGetFileProperties(int clipId, const QMap < QString, QString > &properties, const QMap < QString, QString > &metadata);


  private:
    ProjectListView *listView;
    KTreeWidgetSearchLine *searchView;
    Render *m_render;
    Timecode m_timecode;
    double m_fps;
    QToolBar *m_toolbar;
    QMenu *m_menu;
    KUndoStack *m_commandStack;
    int m_clipIdCounter;
    void selectItemById(const int clipId);
    ProjectItem *getItemById(int id);
    QAction *m_editAction;
    QAction *m_deleteAction;

  private slots:
    void slotAddClip(QUrl givenUrl = QUrl(), const QString &group = QString::null);
    void slotRemoveClip();
    void slotEditClip();
    void slotClipSelected();
    void slotAddColorClip();
    void slotEditClip(QTreeWidgetItem *, int);
    void slotContextMenu( const QPoint &pos, QTreeWidgetItem * );
    void slotAddFolder();
    //void slotShowMenu(const QPoint &pos);



  signals:
    void clipSelected(const QDomElement &);
    void getFileProperties(const QDomElement&, int);
    void receivedClipDuration(int, int);
};

#endif
