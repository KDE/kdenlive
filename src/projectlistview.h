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


#ifndef PROJECTLISTVIEW_H
#define PROJECTLISTVIEW_H

#include <QTreeWidget>
#include <QContextMenuEvent>
#include <QPainter>

class DocClipBase;

class KUrl;

class ProjectListView : public QTreeWidget
{
    Q_OBJECT

public:
    ProjectListView(QWidget *parent = 0);
    virtual ~ProjectListView();
    void processLayout();
    void updateStyleSheet();

protected:
    virtual void contextMenuEvent(QContextMenuEvent * event);
    virtual void mouseDoubleClickEvent(QMouseEvent * event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void dropEvent(QDropEvent *event);
    virtual QStringList mimeTypes() const;
    virtual Qt::DropActions supportedDropActions() const;
    virtual void dragLeaveEvent(QDragLeaveEvent *);

    /** @brief Filters key events to make sure user can expand items with + / -. */
    virtual bool eventFilter(QObject *obj, QEvent *ev);

public slots:


private:
    bool m_dragStarted;
    QPoint m_DragStartPosition;

private slots:
    void configureColumns(const QPoint& pos);
    void slotCollapsed(QTreeWidgetItem *item);
    void slotExpanded(QTreeWidgetItem *item);

signals:
    void requestMenu(const QPoint &, QTreeWidgetItem *);
    void addClip();
    void addClip(const QList <QUrl>, const QString &, const QString &);
    void showProperties(DocClipBase *);
    void focusMonitor(bool forceRefresh);
    void pauseMonitor();
    void addClipCut(const QString&, int, int);
    void projectModified();
};

#endif
