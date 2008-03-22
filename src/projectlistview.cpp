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

#include "QApplication"

#include "KDebug"

#include "projectitem.h"
#include "projectlistview.h"


ProjectListView::ProjectListView(QWidget *parent)
        : QTreeWidget(parent), m_dragStarted(false) {
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDropIndicatorShown(true);
    setAlternatingRowColors(true);
    setDragEnabled(true);
    setAcceptDrops(true);
}

ProjectListView::~ProjectListView() {
}

void ProjectListView::editItem(QTreeWidgetItem * item, int column) {
    kDebug() << "////////////////  EDIT ITEM, COL: " << column;
}

// virtual
void ProjectListView::contextMenuEvent(QContextMenuEvent * event) {
    emit requestMenu(event->globalPos(), itemAt(event->pos()));
}

// virtual
void ProjectListView::mouseDoubleClickEvent(QMouseEvent * event) {
    if (!itemAt(event->pos())) emit addClip();
    else if (columnAt(event->pos().x()) == 2) QTreeWidget::mouseDoubleClickEvent(event);
}

// virtual
void ProjectListView::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls() || event->mimeData()->hasText()) {
        kDebug() << "////////////////  DRAG ENTR OK";
    }
    event->acceptProposedAction();
}

// virtual
void ProjectListView::dropEvent(QDropEvent *event) {
    kDebug() << "////////////////  DROPPED EVENT";
    if (event->mimeData()->hasUrls()) {
        QTreeWidgetItem *item = itemAt(event->pos());
        QString groupName;
        if (item) {
            if (((ProjectItem *) item)->isGroup()) groupName = item->text(1);
            else if (item->parent() && ((ProjectItem *) item->parent())->isGroup())
                groupName = item->parent()->text(1);
        }
        QList <QUrl> list;
        list = event->mimeData()->urls();
        foreach(QUrl url, list) {
            emit addClip(url, groupName);
        }

    } else if (event->mimeData()->hasText()) {
        QTreeWidgetItem *item = itemAt(event->pos());
        if (item) {
            if (item->parent()) item = item->parent();
            if (((ProjectItem *) item)->isGroup()) {
                //emit addClip(event->mimeData->text());
                kDebug() << "////////////////  DROPPED RIGHT 1 ";
                QList <QTreeWidgetItem *> list;
                list = selectedItems();
                ProjectItem *clone;
                foreach(QTreeWidgetItem *it, list) {
                    // TODO allow dragging of folders ?
                    if (!((ProjectItem *) it)->isGroup() && ((ProjectItem *) it)->clipId() < 10000) {
                        if (it->parent()) clone = (ProjectItem*) it->parent()->takeChild(it->parent()->indexOfChild(it));
                        else clone = (ProjectItem*) takeTopLevelItem(indexOfTopLevelItem(it));
                        if (clone) item->addChild(clone);
                    }
                }
            } else item = NULL;
        }
        if (!item) {
            kDebug() << "////////////////  DROPPED ON EMPTY ZONE";
            // item dropped in empty zone, move it to top level
            QList <QTreeWidgetItem *> list;
            list = selectedItems();
            ProjectItem *clone;
            foreach(QTreeWidgetItem *it, list) {
                QTreeWidgetItem *parent = it->parent();
                if (parent && ((ProjectItem *) it)->clipId() < 10000)  {
                    kDebug() << "++ item parent: " << parent->text(1);
                    clone = (ProjectItem*) parent->takeChild(parent->indexOfChild(it));
                    if (clone) addTopLevelItem(clone);
                }
            }
        }
    }
    event->acceptProposedAction();
}

// virtual
void ProjectListView::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        this->m_DragStartPosition = event->pos();
        m_dragStarted = true;
    }
    QTreeWidget::mousePressEvent(event);
}


// virtual
void ProjectListView::mouseMoveEvent(QMouseEvent *event) {
    kDebug() << "// DRAG STARTED, MOUSE MOVED: ";
    if (!m_dragStarted) return;

    if ((event->pos() - m_DragStartPosition).manhattanLength()
            < QApplication::startDragDistance())
        return;

    {
        ProjectItem *clickItem = (ProjectItem *) itemAt(event->pos());
        if (clickItem) {
            QDrag *drag = new QDrag(this);
            QMimeData *mimeData = new QMimeData;
            QDomDocument doc;
            QList <QTreeWidgetItem *> list;
            list = selectedItems();
            QStringList ids;
            foreach(QTreeWidgetItem *item, list) {
                // TODO allow dragging of folders
                if (!((ProjectItem *) item)->isGroup())
                    ids.append(QString::number(((ProjectItem *) item)->clipId()));
            }
            QByteArray data;
            data.append(ids.join(";").toUtf8()); //doc.toString().toUtf8());
            mimeData->setData("kdenlive/producerslist", data);
            //mimeData->setText(ids.join(";")); //doc.toString());
            //mimeData->setImageData(image);
            drag->setMimeData(mimeData);
            drag->setPixmap(clickItem->icon(0).pixmap((int)(50 *16 / 9.0), 50));
            drag->setHotSpot(QPoint(0, 50));
            drag->start(Qt::MoveAction);

            //Qt::DropAction dropAction;
            //dropAction = drag->start(Qt::CopyAction | Qt::MoveAction);

            //Qt::DropAction dropAction = drag->exec();

        }
        //event->accept();
    }
}

void ProjectListView::dragMoveEvent(QDragMoveEvent * event) {
    QTreeWidgetItem * item = itemAt(event->pos());
    event->setDropAction(Qt::IgnoreAction);
    //if (item) {
    event->setDropAction(Qt::MoveAction);
    if (event->mimeData()->hasText()) {
        event->acceptProposedAction();
    }
    //}
}

QStringList ProjectListView::mimeTypes() const {
    QStringList qstrList;
    // list of accepted mime types for drop
    qstrList.append("text/uri-list");
    qstrList.append("text/plain");
    return qstrList;
}


Qt::DropActions ProjectListView::supportedDropActions() const {
    // returns what actions are supported when dropping
    return Qt::MoveAction;
}

#include "projectlistview.moc"
