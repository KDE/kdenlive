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


#include "projectlistview.h"
#include "projectitem.h"
#include "subprojectitem.h"
#include "folderprojectitem.h"
#include "kdenlivesettings.h"

#include <KDebug>
#include <KMenu>
#include <KLocale>

#include <QApplication>
#include <QHeaderView>
#include <QAction>

ProjectListView::ProjectListView(QWidget *parent) :
        QTreeWidget(parent),
        m_dragStarted(false)
{
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDropIndicatorShown(true);
    setAlternatingRowColors(true);
    setDragEnabled(true);
    setAcceptDrops(true);
    setFrameShape(QFrame::NoFrame);
    setRootIsDecorated(true);

    updateStyleSheet();

    setColumnCount(4);
    QStringList headers;
    headers << i18n("Clip") << i18n("Description") << i18n("Rating") << i18n("Date");
    setHeaderLabels(headers);
    setIndentation(12);
    
    QHeaderView* headerView = header();
    headerView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(headerView, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(configureColumns(const QPoint&)));
    connect(this, SIGNAL(itemCollapsed(QTreeWidgetItem *)), this, SLOT(slotCollapsed(QTreeWidgetItem *)));
    connect(this, SIGNAL(itemExpanded(QTreeWidgetItem *)), this, SLOT(slotExpanded(QTreeWidgetItem *)));
    headerView->setClickable(true);
    headerView->setSortIndicatorShown(true);
    headerView->setMovable(false);
    sortByColumn(0, Qt::AscendingOrder);
    setSortingEnabled(true);
    installEventFilter(this);
    if (!KdenliveSettings::showdescriptioncolumn()) hideColumn(1);
    if (!KdenliveSettings::showratingcolumn()) hideColumn(2);
    if (!KdenliveSettings::showdatecolumn()) hideColumn(3);
}

ProjectListView::~ProjectListView()
{
}

void ProjectListView::updateStyleSheet()
{
    QString style = "QTreeView::branch:has-siblings:!adjoins-item{border-image: none;border:0px} \
    QTreeView::branch:has-siblings:adjoins-item {border-image: none;border:0px}      \
    QTreeView::branch:!has-children:!has-siblings:adjoins-item {border-image: none;border:0px} \
    QTreeView::branch:has-children:!has-siblings:closed,QTreeView::branch:closed:has-children:has-siblings {   \
         border-image: none;image: url(:/images/stylesheet-branch-closed.png);}      \
    QTreeView::branch:open:has-children:!has-siblings,QTreeView::branch:open:has-children:has-siblings  {    \
         border-image: none;image: url(:/images/stylesheet-branch-open.png);}";
    setStyleSheet(style);
}

void ProjectListView::processLayout()
{
    executeDelayedItemsLayout();
}

void ProjectListView::configureColumns(const QPoint& pos)
{
    KMenu popup(this);
    popup.addTitle(i18nc("@title:menu", "Columns"));

    QHeaderView* headerView = header();
    for (int i = 1; i < headerView->count(); ++i) {
        const QString text = model()->headerData(i, Qt::Horizontal).toString();
        QAction* action = popup.addAction(text);
        action->setCheckable(true);
        action->setChecked(!headerView->isSectionHidden(i));
        action->setData(i);
    }

    QAction* activatedAction = popup.exec(header()->mapToGlobal(pos));
    if (activatedAction != 0) {
        const bool show = activatedAction->isChecked();

        // remember the changed column visibility in the settings
        const int columnIndex = activatedAction->data().toInt();
        switch (columnIndex) {
        case 1:
            KdenliveSettings::setShowdescriptioncolumn(show);
            break;
        case 2:
            KdenliveSettings::setShowratingcolumn(show);
            break;
        case 3:
            KdenliveSettings::setShowdatecolumn(show);
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
void ProjectListView::contextMenuEvent(QContextMenuEvent * event)
{
    emit requestMenu(event->globalPos(), itemAt(event->pos()));
}

void ProjectListView::slotCollapsed(QTreeWidgetItem *item)
{
    if (item->type() == PROJECTFOLDERTYPE) {
        blockSignals(true);
        static_cast <FolderProjectItem *>(item)->switchIcon();
        blockSignals(false);
    }
}

void ProjectListView::slotExpanded(QTreeWidgetItem *item)
{
    if (item->type() == PROJECTFOLDERTYPE) {
        blockSignals(true);
        static_cast <FolderProjectItem *>(item)->switchIcon();
        blockSignals(false);
    }
}

bool ProjectListView::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) {
        QKeyEvent* ke = (QKeyEvent*) event;
        if (ke->key() == Qt::Key_Plus) {
            if (currentItem()) currentItem()->setExpanded(true);
            event->accept();
            return true;
        } else if (ke->key() == Qt::Key_Minus) {
            if (currentItem()) currentItem()->setExpanded(false);
            event->accept();
            return true;
        } else {
            return false;
        }
    } else {
        // pass the event on to the parent class
        return QTreeWidget::eventFilter(obj, event);
    }
}

// virtual
void ProjectListView::mouseDoubleClickEvent(QMouseEvent * event)
{
    QTreeWidgetItem *it = itemAt(event->pos());
    if (!it) {
	emit pauseMonitor();
        emit addClip();
        return;
    }
    ProjectItem *item;
    if (it->type() == PROJECTFOLDERTYPE) {
        if ((columnAt(event->pos().x()) == 0)) {
            QPixmap pix = qVariantValue<QPixmap>(it->data(0, Qt::DecorationRole));
            int offset = pix.width() + indentation();
            if (event->pos().x() < offset) {
                it->setExpanded(!it->isExpanded());
                event->accept();
            } else QTreeWidget::mouseDoubleClickEvent(event);
        }
        return;
    }
    if (it->type() == PROJECTSUBCLIPTYPE) {
        // subitem
        if ((columnAt(event->pos().x()) == 1)) {
            QTreeWidget::mouseDoubleClickEvent(event);
            return;
        }
        item = static_cast <ProjectItem *>(it->parent());
    } else item = static_cast <ProjectItem *>(it);

    if (!(item->flags() & Qt::ItemIsDragEnabled)) return;

    int column = columnAt(event->pos().x());
    if (column == 0 && (item->clipType() == SLIDESHOW || item->clipType() == TEXT || item->clipType() == COLOR || it->childCount() > 0)) {
        QPixmap pix = qVariantValue<QPixmap>(it->data(0, Qt::DecorationRole));
        int offset = pix.width() + indentation();
        if (item->parent()) offset += indentation();
        if (it->childCount() > 0) {
            if (offset > event->pos().x()) {
                it->setExpanded(!it->isExpanded());
                event->accept();
                return;
            }
        } else if (pix.isNull() || offset < event->pos().x()) {
            QTreeWidget::mouseDoubleClickEvent(event);
            return;
        }
    }
    if ((column == 1) && it->type() != PROJECTSUBCLIPTYPE) {
        QTreeWidget::mouseDoubleClickEvent(event);
        return;
    }
    emit showProperties(item->referencedClip());
}


// virtual
void ProjectListView::dropEvent(QDropEvent *event)
{
    FolderProjectItem *item = NULL;
    QTreeWidgetItem *it = itemAt(event->pos());
    while (it && it->type() != PROJECTFOLDERTYPE) {
        it = it->parent();
    }
    if (it) item = static_cast <FolderProjectItem *>(it);
    if (event->mimeData()->hasUrls()) {
        QString groupName;
        QString groupId;
        if (item) {
            groupName = item->groupName();
            groupId = item->clipId();
        }
        emit addClip(event->mimeData()->urls(), groupName, groupId);
        event->setDropAction(Qt::CopyAction);
        event->accept();
        QTreeWidget::dropEvent(event);
        return;
    } else if (event->mimeData()->hasFormat("kdenlive/producerslist")) {
        if (item) {
            //emit addClip(event->mimeData->text());
            const QList <QTreeWidgetItem *> list = selectedItems();
            ProjectItem *clone;
            QString parentId = item->clipId();
            foreach(QTreeWidgetItem *it, list) {
                // TODO allow dragging of folders ?
                if (it->type() == PROJECTCLIPTYPE) {
                    if (it->parent()) clone = (ProjectItem*) it->parent()->takeChild(it->parent()->indexOfChild(it));
                    else clone = (ProjectItem*) takeTopLevelItem(indexOfTopLevelItem(it));
                    if (clone && item) {
                        item->addChild(clone);
                        QMap <QString, QString> props;
                        props.insert("groupname", item->groupName());
                        props.insert("groupid", parentId);
                        clone->setProperties(props);
                    }
                } else item = NULL;
            }
        } else {
            // item dropped in empty zone, move it to top level
            const QList <QTreeWidgetItem *> list = selectedItems();
            ProjectItem *clone;
            foreach(QTreeWidgetItem *it, list) {
                if (it->type() != PROJECTCLIPTYPE) continue;
                QTreeWidgetItem *parent = it->parent();
                if (parent/* && ((ProjectItem *) it)->clipId() < 10000*/)  {
                    kDebug() << "++ item parent: " << parent->text(1);
                    clone = static_cast <ProjectItem*>(parent->takeChild(parent->indexOfChild(it)));
                    if (clone) {
                        addTopLevelItem(clone);
                        clone->clearProperty("groupname");
                        clone->clearProperty("groupid");
                    }
                }
            }
        }
        emit projectModified();
    } else if (event->mimeData()->hasFormat("kdenlive/clip")) {
        QStringList list = QString(event->mimeData()->data("kdenlive/clip")).split(';');
        emit addClipCut(list.at(0), list.at(1).toInt(), list.at(2).toInt());
    }
    if (event->source() == this) {
	event->setDropAction(Qt::MoveAction);
        event->accept();
    } else {
	event->acceptProposedAction();
    }
    QTreeWidget::dropEvent(event);
}

// virtual
void ProjectListView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_DragStartPosition = event->pos();
        m_dragStarted = true;
        /*QTreeWidgetItem *underMouse = itemAt(event->pos());
        ProjectItem *item = static_cast<ProjectItem *>(underMouse);
        if (item) {
            QRect itemRect = visualItemRect(item);
            if (item->underJobMenu(itemRect, event->pos())) {
                emit display
            }
            
            && underMouse->isSelected()) emit focusMonitor()
        }*/
    }
    QTreeWidget::mousePressEvent(event);
}

// virtual
void ProjectListView::mouseReleaseEvent(QMouseEvent *event)
{
    QTreeWidget::mouseReleaseEvent(event);
    QTreeWidgetItem *underMouse = itemAt(event->pos());
    if (underMouse) emit focusMonitor(true);
}

// virtual
void ProjectListView::mouseMoveEvent(QMouseEvent *event)
{
    //kDebug() << "// DRAG STARTED, MOUSE MOVED: ";
    if (!m_dragStarted) return;

    if ((event->pos() - m_DragStartPosition).manhattanLength()
            < QApplication::startDragDistance())
        return;

    QTreeWidgetItem *it = itemAt(m_DragStartPosition);
    if (!it) return;
    if (it && (it->flags() & Qt::ItemIsDragEnabled)) {
	QDrag *drag = new QDrag(this);
        QMimeData *mimeData = new QMimeData;
        const QList <QTreeWidgetItem *> list = selectedItems();
        QStringList ids;
        foreach(const QTreeWidgetItem *item, list) {
	    if (item->type() == PROJECTFOLDERTYPE) {
		const int children = item->childCount();
                for (int i = 0; i < children; i++) {
		    ids.append(static_cast <ProjectItem *>(item->child(i))->clipId());
                }
	    } else if (item->type() == PROJECTSUBCLIPTYPE) {
		const ProjectItem *parentclip = static_cast <const ProjectItem *>(item->parent());
		const SubProjectItem *clickItem = static_cast <const SubProjectItem *>(item);
		QPoint p = clickItem->zone();
		QString data = parentclip->clipId();
		data.append("/" + QString::number(p.x()));
		data.append("/" + QString::number(p.y()));
		ids.append(data);
            } else {
		const ProjectItem *clip = static_cast <const ProjectItem *>(item);
                ids.append(clip->clipId());
            }
        }
        if (ids.isEmpty()) return;
        QByteArray data;
        data.append(ids.join(";").toUtf8()); //doc.toString().toUtf8());
        mimeData->setData("kdenlive/producerslist", data);
        //mimeData->setText(ids.join(";")); //doc.toString());
        //mimeData->setImageData(image);
        drag->setMimeData(mimeData);
        drag->setPixmap(it->data(0, Qt::DecorationRole).value<QPixmap>());
        drag->setHotSpot(QPoint(0, 40));
        drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
    }
}

// virtual
void ProjectListView::dragLeaveEvent(QDragLeaveEvent *event)
{
    // stop playing because we get a crash otherwise when fetching the thumbnails
    emit pauseMonitor();
    QTreeWidget::dragLeaveEvent(event);
}

QStringList ProjectListView::mimeTypes() const
{
    QStringList qstrList;
    qstrList << QTreeWidget::mimeTypes();
    // list of accepted mime types for drop
    qstrList.append("text/uri-list");
    qstrList.append("text/plain");
    qstrList.append("kdenlive/producerslist");
    qstrList.append("kdenlive/clip");
    return qstrList;
}


Qt::DropActions ProjectListView::supportedDropActions() const
{
    // returns what actions are supported when dropping
    return Qt::MoveAction | Qt::CopyAction;
}

#include "projectlistview.moc"
