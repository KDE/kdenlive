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
#include <QStyledItemDelegate>



class ItemDelegate: public QStyledItemDelegate
{
public:
    enum ItemRole {
        NameRole = Qt::UserRole,
        DurationRole,
        UsageRole
    };

    explicit ItemDelegate(QAbstractItemView* parent = 0)
        : QStyledItemDelegate(parent)
    {
    }

    /*void drawFocus(QPainter *, const QStyleOptionViewItem &, const QRect &) const {
    }*/

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
};

class ProjectListView : public QTreeWidget
{
    Q_OBJECT

public:
    explicit ProjectListView(QWidget *parent = 0);
    ~ProjectListView();
    void processLayout();
    void updateStyleSheet();

protected:
    void contextMenuEvent(QContextMenuEvent * event);
    void mouseDoubleClickEvent(QMouseEvent * event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void dropEvent(QDropEvent *event);
    QStringList mimeTypes() const;
    Qt::DropActions supportedDropActions() const;
    void dragLeaveEvent(QDragLeaveEvent *);

    /** @brief Filters key events to make sure user can expand items with + / -. */
    bool eventFilter(QObject *obj, QEvent *ev);

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
    void addClip(const QList <QUrl> &, const QString &, const QString &);
    void showProperties();
    void focusMonitor(bool forceRefresh);
    void pauseMonitor();
    void addClipCut(const QString&, int, int);
    void projectModified();
};

#endif
