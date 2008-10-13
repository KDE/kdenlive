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

#include <QApplication>
#include <QHeaderView>
#include <QAction>

#include <KDebug>
#include <KMenu>
#include <KLocale>

#include "projectitem.h"
#include "projectlistview.h"
#include "kdenlivesettings.h"


ProjectListView::ProjectListView(QWidget *parent)
        : QTreeWidget(parent), m_dragStarted(false) {
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDropIndicatorShown(true);
    setAlternatingRowColors(true);
    setDragEnabled(true);
    setAcceptDrops(true);

    setColumnCount(4);
    QStringList headers;
    headers << i18n("Thumbnail") << i18n("Filename") << i18n("Description") << i18n("Rating");
    setHeaderLabels(headers);
    sortByColumn(1, Qt::AscendingOrder);

    QHeaderView* headerView = header();
    headerView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(headerView, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(configureColumns(const QPoint&)));

    //connect(this, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)), this, SLOT(slotFocusOut(QTreeWidgetItem *, QTreeWidgetItem *)));

    if (!KdenliveSettings::showdescriptioncolumn()) hideColumn(2);
    if (!KdenliveSettings::showratingcolumn()) hideColumn(3);

    setIconSize(QSize(60, 40));
    setSortingEnabled(true);
}

ProjectListView::~ProjectListView() {
}


void ProjectListView::configureColumns(const QPoint& pos) {
    KMenu popup(this);
    popup.addTitle(i18nc("@title:menu", "Columns"));

    QHeaderView* headerView = header();
    for (int i = 2; i < headerView->count(); ++i) {
        const int logicalIndex = headerView->logicalIndex(i);
        const QString text = model()->headerData(i, Qt::Horizontal).toString();
        QAction* action = popup.addAction(text);
        action->setCheckable(true);
        action->setChecked(!headerView->isSectionHidden(logicalIndex));
        action->setData(i);
    }

    QAction* activatedAction = popup.exec(header()->mapToGlobal(pos));
    if (activatedAction != 0) {
        const bool show = activatedAction->isChecked();

        // remember the changed column visibility in the settings
        const int columnIndex = activatedAction->data().toInt();
        switch (columnIndex) {
        case 2:
            KdenliveSettings::setShowdescriptioncolumn(show);
            break;
        case 3:
            KdenliveSettings::setShowratingcolumn(show);
            break;
        default:
            break;
        }

        // apply the changed column visibility
        if (show) {
            showColumn(columnIndex);
        } else {
            hideColumn(columnIndex);
        }
    }
}

// virtual
void ProjectListView::contextMenuEvent(QContextMenuEvent * event) {
    emit requestMenu(event->globalPos(), itemAt(event->pos()));
}

// virtual
void ProjectListView::mouseDoubleClickEvent(QMouseEvent * event) {
    ProjectItem *item = static_cast <ProjectItem *>(itemAt(event->pos()));
    if (!item) emit addClip();
    else if (item->isGroup()) {
        if ((columnAt(event->pos().x()) == 1)) QTreeWidget::mouseDoubleClickEvent(event);
    } else emit showProperties(item->referencedClip());
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
        const QList <QUrl> list = event->mimeData()->urls();
        foreach(const QUrl &url, list) {
            emit addClip(url, groupName);
        }

    } else if (event->mimeData()->hasFormat("kdenlive/producerslist")) {
        ProjectItem *item = static_cast <ProjectItem *>(itemAt(event->pos()));
        if (item) {
            if (item->parent()) item = static_cast <ProjectItem *>(item->parent());
            if (item->isGroup()) {
                //emit addClip(event->mimeData->text());
                kDebug() << "////////////////  DROPPED RIGHT 1 ";
                const QList <QTreeWidgetItem *> list = selectedItems();
                ProjectItem *clone;
                QString parentId = item->clipId();
                foreach(QTreeWidgetItem *it, list) {
                    // TODO allow dragging of folders ?
                    if (!((ProjectItem *) it)->isGroup()/* && ((ProjectItem *) it)->clipId() < 10000*/) {
                        if (it->parent()) clone = (ProjectItem*) it->parent()->takeChild(it->parent()->indexOfChild(it));
                        else clone = (ProjectItem*) takeTopLevelItem(indexOfTopLevelItem(it));
                        if (clone) {
                            item->addChild(clone);
                            QMap <QString, QString> props;
                            props.insert("groupname", item->groupName());
                            props.insert("groupid", parentId);
                            clone->setProperties(props);
                        }
                    }
                }
            } else item = NULL;
        }
        if (!item) {
            kDebug() << "////////////////  DROPPED ON EMPTY ZONE";
            // item dropped in empty zone, move it to top level
            const QList <QTreeWidgetItem *> list = selectedItems();
            ProjectItem *clone;
            foreach(QTreeWidgetItem *it, list) {
                QTreeWidgetItem *parent = it->parent();
                if (parent/* && ((ProjectItem *) it)->clipId() < 10000*/)  {
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
        QTreeWidgetItem *underMouse = itemAt(event->pos());
        if (underMouse && underMouse->isSelected()) emit focusMonitor();
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
        ProjectItem *clickItem = (ProjectItem *) itemAt(m_DragStartPosition); //event->pos());
        if (clickItem) {
            QDrag *drag = new QDrag(this);
            QMimeData *mimeData = new QMimeData;
            QDomDocument doc;
            const QList <QTreeWidgetItem *> list = selectedItems();
            QStringList ids;
            foreach(const QTreeWidgetItem *item, list) {
		const ProjectItem *clip = static_cast <const ProjectItem *> (item);
		if (!clip->isGroup()) ids.append(clip->clipId());
		else {
		    const int children = item->childCount();
		    for (int i = 0; i < children; i++) {
			ids.append(static_cast <ProjectItem *>(item->child(i))->clipId());
		    }
		}
            }
	    if (ids.isEmpty()) return;
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

// virtual
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
    qstrList << QTreeWidget::mimeTypes();
    // list of accepted mime types for drop
    qstrList.append("text/uri-list");
    qstrList.append("text/plain");
    qstrList.append("kdenlive/producerslist");
    return qstrList;
}


Qt::DropActions ProjectListView::supportedDropActions() const {
    // returns what actions are supported when dropping
    return Qt::MoveAction;
}

#include "projectlistview.moc"
