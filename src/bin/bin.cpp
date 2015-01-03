/*
Copyright (C) 2012  Till Theato <root@ttill.de>
Copyright (C) 2014  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy 
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "bin.h"
#include "mainwindow.h"
#include "projectitemmodel.h"
#include "projectclip.h"
#include "projectfolder.h"
#include "kdenlivesettings.h"
#include "project/projectmanager.h"
#include "project/clipmanager.h"
#include "project/jobs/jobmanager.h"
#include "monitor/monitor.h"
#include "doc/kdenlivedoc.h"
#include "dialogs/clipcreationdialog.h"
#include "core.h"
#include "mltcontroller/clipcontroller.h"
#include "projectsortproxymodel.h"
#include "mlt++/Mlt.h"


#include <KToolBar>

#include <QVBoxLayout>
#include <QSlider>
#include <QMenu>
#include <QDebug>
#include <QTableWidget>
#include <KSplitterCollapserButton>

EventEater::EventEater(QObject *parent) : QObject(parent)
{
}

bool EventEater::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        emit focusClipMonitor();
        return QObject::eventFilter(obj, event);
    }
    if (event->type() == QEvent::MouseButtonDblClick) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        QAbstractItemView *view = qobject_cast<QAbstractItemView*>(obj->parent());
        if (view) {
            QModelIndex idx = view->indexAt(mouseEvent->pos());
            if (idx == QModelIndex()) {
                // User double clicked on empty area
                emit addClip();
            }
            else {
		emit editItem(idx);
            }
        }
        else {
            qDebug()<<" +++++++ NO VIEW-------!!";
        }
        return true;
    } else {
        return QObject::eventFilter(obj, event);
    }
}


Bin::Bin(QWidget* parent) :
    QWidget(parent)
  , m_itemModel(NULL)
  , m_itemView(NULL)
  , m_listType(BinTreeView)
  , m_jobManager(NULL)
  , m_rootFolder(NULL)
  , m_doc(NULL)
  , m_iconSize(160, 90)
  , m_propertiesPanel(NULL)
{
    // TODO: proper ui, search line, add menu, ...
    QVBoxLayout *layout = new QVBoxLayout(this);

    // Create toolbar for buttons
    m_toolbar = new KToolBar(this);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolbar->setIconDimensions(style()->pixelMetric(QStyle::PM_SmallIconSize));
    layout->addWidget(m_toolbar);

    // Search line
    m_proxyModel = new ProjectSortProxyModel(this);
    m_proxyModel->setDynamicSortFilter(true);
    QLineEdit *searchLine = new QLineEdit(this);
    searchLine->setClearButtonEnabled(true);
    connect(searchLine, SIGNAL(textChanged(const QString &)), m_proxyModel, SLOT(slotSetSearchString(const QString &)));
    m_toolbar->addWidget(searchLine);

    // Build item view model
    m_itemModel = new ProjectItemModel(this);

    // Connect models
    m_proxyModel->setSourceModel(m_itemModel);
    connect(m_itemModel, SIGNAL(dataChanged(const QModelIndex&,const QModelIndex&)), m_proxyModel, SLOT(slotDataChanged(const QModelIndex&,const
    QModelIndex&)));
    connect(m_itemModel, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(rowsInserted(QModelIndex,int,int)));
    connect(m_itemModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(rowsRemoved(QModelIndex,int,int)));
    connect(m_proxyModel, SIGNAL(selectModel(QModelIndex)), this, SLOT(selectProxyModel(QModelIndex)));
    connect(m_itemModel, SIGNAL(markersNeedUpdate(QString,QList<int>)), this, SLOT(slotMarkersNeedUpdate(QString,QList<int>)));
    connect(m_itemModel, SIGNAL(itemDropped(QStringList, const QModelIndex &)), this, SLOT(slotItemDropped(QStringList, const QModelIndex &)));
    connect(m_itemModel, SIGNAL(itemDropped(const QList<QUrl>&, const QModelIndex &)), this, SLOT(slotItemDropped(const QList<QUrl>&, const QModelIndex &)));

    // Zoom slider
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setMaximumWidth(100);
    m_slider->setRange(0, 10);
    m_slider->setValue(4);
    connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(slotSetIconSize(int)));
    m_toolbar->addWidget(m_slider);

    // View type
    KSelectAction *listType = new KSelectAction(QIcon::fromTheme("view-list-tree"), i18n("View Mode"), this);
    QAction *treeViewAction = listType->addAction(QIcon::fromTheme("view-list-tree"), i18n("Tree View"));
    treeViewAction->setData(BinTreeView);
    if (m_listType == treeViewAction->data().toInt()) {
        listType->setCurrentAction(treeViewAction);
    }
    QAction *iconViewAction = listType->addAction(QIcon::fromTheme("view-list-icons"), i18n("Icon View"));
    iconViewAction->setData(BinIconView);
    if (m_listType == iconViewAction->data().toInt()) {
        listType->setCurrentAction(iconViewAction);
    }
    listType->setToolBarMode(KSelectAction::MenuMode);
    connect(listType, SIGNAL(triggered(QAction*)), this, SLOT(slotInitView(QAction*)));
    m_toolbar->addAction(listType);
    
    /*m_addButton = new QToolButton;
    m_addButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_addButton->setAutoRaise(true);
    m_addButton->setIconSize(iconSize);
    box->addWidget(m_addButton);

    m_editButton = new QToolButton;
    m_editButton->setAutoRaise(true);
    m_editButton->setIconSize(iconSize);
    box->addWidget(m_editButton);

    m_deleteButton = new QToolButton;
    m_deleteButton->setAutoRaise(true);
    m_deleteButton->setIconSize(iconSize);
    box->addWidget(m_deleteButton);
    frame->setLayout(box);
    layout->addWidget(frame);*/

    m_eventEater = new EventEater(this);
    connect(m_eventEater, SIGNAL(addClip()), this, SLOT(slotAddClip()));
    connect(m_eventEater, SIGNAL(deleteSelectedClips()), this, SLOT(slotDeleteClip()));
    m_binTreeViewDelegate = new BinItemDelegate(this);
    //connect(pCore->projectManager(), SIGNAL(projectOpened(Project*)), this, SLOT(setProject(Project*)));
    m_splitter = new QSplitter(this);
    m_headerInfo = QByteArray::fromBase64(KdenliveSettings::treeviewheaders().toLatin1());

    connect(m_eventEater, SIGNAL(editItem(QModelIndex)), this, SLOT(slotSwitchClipProperties(QModelIndex)), Qt::UniqueConnection);
    connect(m_eventEater, SIGNAL(showMenu(QString)), this, SLOT(showClipMenu(QString)), Qt::UniqueConnection);

    layout->addWidget(m_splitter);
    m_propertiesPanel = new QWidget(m_splitter);
    QVBoxLayout *lay = new QVBoxLayout;
    m_propertiesPanel->setLayout(lay);
    m_propertiesTable = new QTableWidget(this);
    m_propertiesTable->setColumnCount(2);
    QHeaderView *header = m_propertiesTable->horizontalHeader();
    header->setStretchLastSection(true);
    lay->addWidget(m_propertiesTable);

    m_splitter->addWidget(m_propertiesPanel);
    m_collapser = new KSplitterCollapserButton(m_propertiesPanel, m_splitter);
    connect(m_collapser, SIGNAL(clicked(bool)), this, SLOT(slotRefreshClipProperties()));
}

Bin::~Bin()
{
}

void Bin::slotSaveHeaders()
{
    if (m_itemView && m_listType == BinTreeView) {
        // save current treeview state (column width)
        QTreeView *view = static_cast<QTreeView*>(m_itemView);
        m_headerInfo = view->header()->saveState();
        KdenliveSettings::setTreeviewheaders(m_headerInfo.toBase64());
    }
}

Monitor *Bin::monitor()
{
    return m_monitor;
}

const QStringList Bin::getFolderInfo()
{
    QStringList folderInfo;
    QModelIndex ix = m_proxyModel->selectionModel()->currentIndex();
    if (ix.isValid() && m_proxyModel->selectionModel()->isSelected(ix)) {
        AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(ix).internalPointer());
        while (!currentItem->isFolder()) {
            currentItem = currentItem->parent();
        }
        if (currentItem == m_rootFolder) {
            // clip was added to root folder, leave folder info empty
        } else {
            folderInfo << currentItem->name();
            folderInfo << currentItem->clipId();
        }
    }
    return folderInfo;
}

void Bin::slotAddClip()
{
    // Check if we are in a folder
    QStringList folderInfo = getFolderInfo();
    ClipCreationDialogDialog::createClipsCommand(pCore->projectManager()->current(), folderInfo, this);
}

void Bin::deleteClip(const QString &id)
{
    if (m_monitor->activeClipId() == id) {
        m_monitor->openClip(NULL);
    }
    ProjectClip *clip = m_rootFolder->clip(id);
    if (!clip) return;
    m_jobManager->discardJobs(id);
    m_rootFolder->removeChild(clip);
    delete clip;
}

void Bin::slotDeleteClip()
{
    QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    QStringList ids;
    foreach (const QModelIndex &ix, indexes) {
	ProjectClip *currentItem = static_cast<ProjectClip *>(m_proxyModel->mapToSource(ix).internalPointer());
	if (currentItem) {
	    ids << currentItem->clipId();
	}
    }
    // For some reason, we get duplicates, which is not expected
    //ids.removeDuplicates();
    pCore->projectManager()->deleteProjectClips(ids);
}

void Bin::slotReloadClip()
{
    QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    foreach (const QModelIndex &ix, indexes) {
        if (!ix.isValid()) {
            continue;
        }
        AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(ix).internalPointer());
        if (currentItem && !currentItem->isFolder()) {
            m_monitor->openClip(NULL);
            QDomDocument doc;
            QDomElement xml = currentItem->toXml(doc);
            currentItem->setClipStatus(AbstractProjectItem::StatusWaiting);
            
            // We need to set a temporary id before all outdated producers are replaced;
            pCore->projectManager()->current()->renderer()->getFileProperties(xml, currentItem->clipId(), 150, true);
        }
    }
}

ProjectFolder *Bin::rootFolder()
{
    return m_rootFolder;
}

double Bin::projectRatio()
{
    return m_doc->dar();
}

void Bin::setMonitor(Monitor *monitor)
{
    m_monitor = monitor;
    connect(m_eventEater, SIGNAL(focusClipMonitor()), m_monitor, SLOT(slotActivateMonitor()), Qt::UniqueConnection);
}

int Bin::getFreeFolderId()
{
    return m_folderCounter++;
}

int Bin::getFreeClipId()
{
    return m_clipCounter++;
}

int Bin::lastClipId() const
{
    return qMax(0, m_clipCounter - 1);
}

void Bin::setDocument(KdenliveDoc* project)
{
    // Remove clip from Bin's monitor
    m_monitor->openClip(NULL);
    closeEditing();
    setEnabled(false);
    delete m_rootFolder;
    delete m_itemView;
    m_itemView = NULL;
    delete m_jobManager;
    m_clipCounter = 1;
    m_folderCounter = 1;
    m_doc = project;
    int iconHeight = style()->pixelMetric(QStyle::PM_ToolBarIconSize) * 2;
    m_iconSize = QSize(iconHeight * m_doc->dar(), iconHeight);
    m_itemModel->setIconSize(m_iconSize);
    m_jobManager = new JobManager(this, project->fps());
    m_rootFolder = new ProjectFolder(this);
    setEnabled(true);
    connect(this, SIGNAL(producerReady(QString)), m_doc->renderer(), SLOT(slotProcessingDone(QString)));
    //connect(m_itemModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), m_itemView
    //connect(m_itemModel, SIGNAL(updateCurrentItem()), this, SLOT(autoSelect()));
    slotInitView(NULL);
    autoSelect();
}

void Bin::createClip(QDomElement xml)
{
    // Check if clip should be in a folder
    QString groupId = xml.attribute("groupid");
    ProjectFolder *parentFolder = m_rootFolder;
    if (!groupId.isEmpty()) {
        QString groupName = xml.attribute("group");
        parentFolder = m_rootFolder->folder(groupId);
        if (!parentFolder) {
            // parent folder does not exist, put in root folder
            parentFolder = m_rootFolder;
        }
    }
    ProjectClip *newItem = new ProjectClip(xml, parentFolder);
}

void Bin::slotAddFolder()
{
    // Check parent item
    QString folderName;
    QString folderId;
    QModelIndex ix = m_proxyModel->selectionModel()->currentIndex();
    ProjectFolder *parentFolder  = m_rootFolder;
    if (ix.isValid() && m_proxyModel->selectionModel()->isSelected(ix)) {
        AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(ix).internalPointer());
        while (!currentItem->isFolder()) {
            currentItem = currentItem->parent();
        }
        if (currentItem->isFolder()) {
            parentFolder = static_cast<ProjectFolder *>(currentItem);
        }
        if (parentFolder != m_rootFolder) {
            // clip was added to a sub folder, set info
            folderName = currentItem->name();
            folderId = currentItem->clipId();
        }
    }
    ProjectFolder *newItem = new ProjectFolder(QString::number(getFreeFolderId()), i18n("Folder"), parentFolder);
}

void Bin::emitAboutToAddItem(AbstractProjectItem* item)
{
    m_itemModel->onAboutToAddItem(item);
}

void Bin::emitItemAdded(AbstractProjectItem* item)
{
    m_itemModel->onItemAdded(item);
}

void Bin::emitAboutToRemoveItem(AbstractProjectItem* item)
{
    m_itemModel->onAboutToRemoveItem(item);
}

void Bin::emitItemRemoved(AbstractProjectItem* item)
{
    m_itemModel->onItemRemoved(item);
}

void Bin::rowsInserted(const QModelIndex &/*parent*/, int /*start*/, int end)
{
    QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    if (indexes.isEmpty()) {
      const QModelIndex id = m_itemModel->index(end, 0, QModelIndex());
      m_proxyModel->selectionModel()->select(m_proxyModel->mapFromSource(id), QItemSelectionModel::Select);
    }
    //selectModel(id);
}

void Bin::rowsRemoved(const QModelIndex &/*parent*/, int start, int /*end*/)
{
    const QModelIndex id = m_itemModel->index(start, 0, QModelIndex());
    m_proxyModel->selectionModel()->select(m_proxyModel->mapFromSource(id), QItemSelectionModel::Select);
    //selectModel(id);
}

void Bin::selectModel(const QModelIndex &id)
{
    m_proxyModel->selectionModel()->select(m_proxyModel->mapFromSource(id), QItemSelectionModel::Select);
    /*if (id.isValid()) {
        AbstractProjectItem *currentItem = static_cast<AbstractProjectItem*>(id.internalPointer());
        if (currentItem) {
            //m_openedProducer = currentItem->clipId();
        }
    }*/
}

void Bin::selectProxyModel(const QModelIndex &id)
{
    if (id.isValid()) {
        AbstractProjectItem *currentItem = static_cast<AbstractProjectItem*>(m_proxyModel->mapToSource(id).internalPointer());
	if (currentItem) {
            // Set item as current so that it displays its content in clip monitor
            currentItem->setCurrent(true);
            if (!currentItem->isFolder()) {
                m_editAction->setEnabled(true);
                m_reloadAction->setEnabled(true);
                if (m_propertiesPanel->width() > 0) {
                    // if info panel is displayed, update info
                    showClipProperties((ProjectClip *)currentItem);
                    m_deleteAction->setText(i18n("Delete Clip"));
                    m_proxyAction->setText(i18n("Proxy Clip"));
                }
            } else {
                // A folder was selected, disable editing clip
                m_editAction->setEnabled(false);
                m_reloadAction->setEnabled(false);
                m_deleteAction->setText(i18n("Delete Folder"));
                m_proxyAction->setText(i18n("Proxy Folder"));
            }
	    m_deleteAction->setEnabled(true);
        } else {
            m_reloadAction->setEnabled(false);
	    m_editAction->setEnabled(false);
	    m_deleteAction->setEnabled(false);
	}
    }
    else {
	m_editAction->setEnabled(false);
	m_deleteAction->setEnabled(false);
	// Display black bg in clip monitor
	//pCore->projectManager()->current()->bin()->monitor()->open(NULL, ClipMonitor);	
    }
}

void Bin::autoSelect()
{
    /*QModelIndex current = m_proxyModel->selectionModel()->currentIndex();
    AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(current).internalPointer());
    if (!currentItem) {
        QModelIndex id = m_proxyModel->index(0, 0, QModelIndex());
        //selectModel(id);
        //m_proxyModel->selectionModel()->select(m_proxyModel->mapFromSource(id), QItemSelectionModel::Select);
    }*/
}

QList <ProjectClip *> Bin::selectedClips()
{
    //TODO: handle clips inside folders
    QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
    QList <ProjectClip *> list;
    foreach (const QModelIndex &ix, indexes) {
	ProjectClip *currentItem = static_cast<ProjectClip *>(m_proxyModel->mapToSource(ix).internalPointer());
	if (currentItem) {
	    list << currentItem;
	}
    }
    return list;
}

void Bin::slotInitView(QAction *action)
{
    closeEditing();
    if (action) {
        int viewType = action->data().toInt();
        if (viewType == m_listType) {
            return;
        }
        if (m_listType == BinTreeView) {
            // save current treeview state (column width)
            QTreeView *view = static_cast<QTreeView*>(m_itemView);
            m_headerInfo = view->header()->saveState();
        }
        m_listType = static_cast<BinViewType>(viewType);
    }

    if (m_itemView) {
        delete m_itemView;
    }

    switch (m_listType) {
    case BinIconView:
        m_itemView = new QListView(m_splitter);
        break;
    default:
        m_itemView = new QTreeView(m_splitter);
        break;
    }
    m_itemView->setMouseTracking(true);
    m_itemView->viewport()->installEventFilter(m_eventEater);
    QSize zoom = m_iconSize;
    zoom = zoom * (m_slider->value() / 4.0);
    m_itemView->setIconSize(zoom);
    m_itemView->setModel(m_proxyModel);
    m_itemView->setSelectionModel(m_proxyModel->selectionModel());
    m_splitter->addWidget(m_itemView);
    m_splitter->insertWidget(2, m_propertiesPanel);
    m_splitter->setSizes(QList <int>() << 4 << 2);
    m_collapser->collapse();

    // setup some default view specific parameters
    if (m_listType == BinTreeView) {
        m_itemView->setItemDelegate(m_binTreeViewDelegate);
        QTreeView *view = static_cast<QTreeView*>(m_itemView);
	view->setSortingEnabled(true);
	view->setHeaderHidden(true);
        if (!m_headerInfo.isEmpty()) {
            view->header()->restoreState(m_headerInfo);
	} else {
            view->header()->resizeSections(QHeaderView::ResizeToContents);
	}
	connect(view->header(), SIGNAL(sectionResized(int,int,int)), this, SLOT(slotSaveHeaders()));
    }
    else if (m_listType == BinIconView) {
	QListView *view = static_cast<QListView*>(m_itemView);
	view->setViewMode(QListView::IconMode);
	view->setMovement(QListView::Static);
	view->setResizeMode(QListView::Adjust);
	view->setUniformItemSizes(true);
    }
    m_itemView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_itemView->setDragDropMode(QAbstractItemView::DragDrop);
    m_itemView->setAcceptDrops(true);
}

void Bin::slotSetIconSize(int size)
{
    if (!m_itemView) {
        return;
    }
    QSize zoom = m_iconSize;
    zoom = zoom * (size / 4.0);
    m_itemView->setIconSize(zoom);
}


void Bin::slotMarkersNeedUpdate(const QString &id, const QList<int> &markers)
{
    // Check if we have a clip timeline that needs update
    /*TimelineWidget *tml = pCore->window()->getTimeline(id);
    if (tml) {
        tml->updateMarkers(markers);
    }*/
    // Update clip monitor
}

void Bin::closeEditing()
{
    //delete m_propertiesPanel;
    //m_propertiesPanel = NULL;
}


void Bin::contextMenuEvent(QContextMenuEvent *event)
{
    bool enableClipActions = false;
    if (m_itemView) {
        QModelIndex idx = m_itemView->indexAt(m_itemView->viewport()->mapFromGlobal(event->globalPos()));
        if (idx != QModelIndex()) {
	    // User right clicked on a clip
            AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(m_proxyModel->mapToSource(idx).internalPointer());
            if (currentItem) {
		enableClipActions = true;
		m_proxyAction->blockSignals(true);
		if (!currentItem->isFolder()) m_proxyAction->setChecked(((ProjectClip *)currentItem)->hasProxy());
		m_proxyAction->blockSignals(false);
            }
        }
    }
    // Actions are enabled / disabled on clip selection changes that is triggered by the model
    m_menu->exec(event->globalPos());
}


void Bin::slotRefreshClipProperties()
{
    QModelIndex current = m_proxyModel->selectionModel()->currentIndex();
    if (current.isValid() && m_proxyModel->selectionModel()->isSelected(current)) {
        ProjectClip *clip = static_cast<ProjectClip *>(m_proxyModel->mapToSource(current).internalPointer());
        if (clip && !clip->isFolder()) {
            showClipProperties(clip);
        }
    }
}

void Bin::slotSwitchClipProperties()
{
    QModelIndex current = m_proxyModel->selectionModel()->currentIndex();
    slotSwitchClipProperties(current);
}

void Bin::slotSwitchClipProperties(const QModelIndex &ix)
{
    if (ix.isValid()) {
        if (m_collapser->isWidgetCollapsed()) {
            ProjectClip *clip = static_cast<ProjectClip *>(m_proxyModel->mapToSource(ix).internalPointer());
            if (clip && !clip->isFolder()) {
                m_collapser->restore();
                showClipProperties(clip);
            }
            else if (clip->isFolder() && m_listType == BinIconView) {
                // Double clicking on a folder enters it in icon view
                m_itemView->setRootIndex(ix);
            }
        }
        else m_collapser->collapse();
    }
    else m_collapser->collapse();
}

void Bin::showClipProperties(ProjectClip *clip)
{
    closeEditing();
    if (!clip || m_propertiesPanel->width() == 0) return;
    QMap <QString, QString> props = clip->properties();
    m_propertiesTable->clearContents();
    if (props.isEmpty()) {
        // Producer for this clip is not ready yet or no properties in this clip
        return;
    }
    // TODO: Build proper clip properties widget
    //PropertiesView *view = new PropertiesView(clip, m_propertiesPanel);

    m_propertiesTable->setRowCount(props.size());
    m_propertiesTable->horizontalHeader()->hide();
    m_propertiesTable->verticalHeader()->hide();
    QTableWidgetItem *keyitem;
    QTableWidgetItem *valueitem;
    QMapIterator<QString, QString> i(props);
    int ix = 0;
    while (i.hasNext()) {
        i.next();
        keyitem = new QTableWidgetItem(i.key());
        m_propertiesTable->setItem(ix, 0, keyitem);
        valueitem = new QTableWidgetItem(i.value());
        m_propertiesTable->setItem(ix, 1, valueitem);
        ix++;
    }
    //m_editedProducer= new Producer(producer, desc, pCore->clipPluginManager());
    //connect(m_editedProducer, SIGNAL(updateClip()), this, SLOT(refreshEditedClip()));
    //connect(m_editedProducer, SIGNAL(reloadClip(QString)), this, SLOT(reloadClip(QString)));
    //connect(m_editedProducer, SIGNAL(editingDone()), this, SLOT(closeEditing()));
}

void Bin::reloadClip(const QString &id)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (!clip) return;
    QDomDocument doc;
    QDomElement xml = clip->toXml(doc);
    pCore->projectManager()->current()->renderer()->getFileProperties(xml, id, 150, true);
}

void Bin::refreshEditedClip()
{
    const QString id = m_propertiesPanel->property("clipId").toString();
    /*pCore->projectManager()->current()->bin()->refreshThumnbail(id);
    pCore->projectManager()->current()->binMonitor()->refresh();*/
}

void Bin::slotThumbnailReady(const QString &id, const QImage &img)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (clip) clip->setThumbnail(img);
}

ProjectClip *Bin::getBinClip(const QString &id)
{
    ProjectClip *clip;
    if (id.contains("_")) {
        clip = m_rootFolder->clip(id.section("_", 0, 0));
    }
    else {
        clip = m_rootFolder->clip(id);
    }
    return clip;
}

void Bin::setWaitingStatus(const QString &id)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (clip) clip->setClipStatus(AbstractProjectItem::StatusWaiting);
}

void Bin::slotProducerReady(requestClipInfo info, ClipController *controller)
{
    ProjectClip *clip = m_rootFolder->clip(info.clipId);
    if (clip) {
        if (!clip->hasProxy()) {
            // Check for file modifications
            ClipType t = clip->clipType();
            if (t == AV || t == Audio || t == Image || t == Video || t == Playlist) {
                m_doc->watchFile(clip->url());
            }
        }
	if (controller) clip->setProducer(controller, info.replaceProducer);
        QString currentClip = m_monitor->activeClipId();
        if (currentClip.isEmpty()) {
            //No clip displayed in monitor, check if item is selected
            QModelIndexList indexes = m_proxyModel->selectionModel()->selectedIndexes();
            foreach (const QModelIndex &ix, indexes) {
                ProjectClip *currentItem = static_cast<ProjectClip *>(m_proxyModel->mapToSource(ix).internalPointer());
                if (currentItem->clipId() == info.clipId) {
                    // Item was selected, show it in monitor
                    currentItem->setCurrent(true);
                    break;
                }
            }
        }
        else if (currentClip == info.clipId) {
            m_monitor->openClip(NULL);
            clip->setCurrent(true);
            //m_monitor->openClip(controller);
        }
    }
    else {
	// Clip not found, create it
        QString groupId = controller->property("groupid");
        ProjectFolder *parentFolder;
        if (!groupId.isEmpty()) {
            QString groupName = controller->property("group");
            parentFolder = m_rootFolder->folder(groupId);
            if (!parentFolder) {
                // parent folder does not exist, put in root folder
                parentFolder = m_rootFolder;
            }
            if (groupId.toInt() >= m_folderCounter) m_folderCounter = groupId.toInt() + 1;
        }
        else parentFolder = m_rootFolder;
        ProjectClip *newItem = new ProjectClip(info.clipId, controller, parentFolder);
        if (info.clipId.toInt() >= m_clipCounter) m_clipCounter = info.clipId.toInt() + 1;
    }
    emit producerReady(info.clipId);
}

void Bin::openProducer(ClipController *controller)
{
    m_monitor->openClip(controller);
}

void Bin::emitItemUpdated(AbstractProjectItem* item)
{
    emit itemUpdated(item);
}

void Bin::setupMenu(QMenu *addMenu, QAction *defaultAction)
{
    QList <QAction *> actions = addMenu->actions();
    for (int i = 0; i < actions.count(); ++i) {
        if (actions.at(i)->data().toString() == "clip_properties") {
	    m_editAction = actions.at(i);
	    m_toolbar->addAction(m_editAction);
            //m_editButton->setDefaultAction(actions.at(i));
            actions.removeAt(i);
            --i;
        } else if (actions.at(i)->data().toString() == "delete_clip") {
	    m_deleteAction = actions.at(i);
	    m_toolbar->addAction(m_deleteAction);
            //m_deleteButton->setDefaultAction(actions.at(i));
            actions.removeAt(i);
            --i;
        } else if (actions.at(i)->data().toString() == "edit_clip") {
            m_openAction = actions.at(i);
            actions.removeAt(i);
            --i;
        } else if (actions.at(i)->data().toString() == "reload_clip") {
            m_reloadAction = actions.at(i);
            actions.removeAt(i);
            --i;
        } else if (actions.at(i)->data().toString() == "proxy_clip") {
            m_proxyAction = actions.at(i);
            actions.removeAt(i);
            --i;
        }
    }

    QMenu *m = new QMenu;
    m->addActions(actions);
    QToolButton *addButton = new QToolButton;
    addButton->setMenu(m);
    addButton->setDefaultAction(defaultAction);
    addButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_toolbar->addWidget(addButton);
    m_menu = new QMenu();
    m_menu->addActions(addMenu->actions());
}

const QString Bin::getDocumentProperty(const QString &key)
{
    return m_doc->getDocumentProperty(key);
}

const QSize Bin::getRenderSize()
{
    return m_doc->getRenderSize();
}

JobManager *Bin::jobManager()
{
    return m_jobManager;
}

void Bin::updateJobStatus(const QString&id, int jobType, int status, const QString &label, const QString &actionName, const QString &details)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (clip) {
        clip->setJobStatus((AbstractClipJob::JOBTYPE) jobType, (ClipJobStatus) status);
    }
}

void Bin::gotProxy(const QString &id)
{
    ProjectClip *clip = m_rootFolder->clip(id);
    if (clip) {
        QDomDocument doc;
        QDomElement xml = clip->toXml(doc);
        pCore->projectManager()->current()->renderer()->getFileProperties(xml, id, 150, true);
    }
}

void Bin::reloadProducer(const QString &id, QDomElement xml)
{
    pCore->projectManager()->current()->renderer()->getFileProperties(xml, id, 150, true);
}

void Bin::discardJobs(const QString &id, AbstractClipJob::JOBTYPE type)
{
    m_jobManager->discardJobs(id, type);
}

void Bin::startJob(const QString &id, AbstractClipJob::JOBTYPE type)
{
    m_jobManager->startJob(id, type);
}

bool Bin::hasPendingJob(const QString &id, AbstractClipJob::JOBTYPE type)
{
    return m_jobManager->hasPendingJob(id, type);
}

void Bin::slotCreateProjectClip()
{
    QAction* act = qobject_cast<QAction *>(sender());
    if (act == 0) {
        // Cannot access triggering action, something is wrong
        qDebug()<<"// Error in clip creation action";
        return;
    }
    ClipType type = (ClipType) act->data().toInt();
    QStringList folderInfo = getFolderInfo();
    switch (type) {
      case Color:
          ClipCreationDialogDialog::createColorClip(pCore->projectManager()->current(), folderInfo, this);
          break;
      case SlideShow:
          ClipCreationDialogDialog::createSlideshowClip(pCore->projectManager()->current(), folderInfo, this);
          break;
      case Text:
          ClipCreationDialogDialog::createTitleClip(pCore->projectManager()->current(), folderInfo, QString(), this);
          break;
      default:
          break;
    }
}

void Bin::slotItemDropped(QStringList ids, const QModelIndex &parent)
{
    AbstractProjectItem *parentItem;
    if (parent.isValid()) {
        parentItem = static_cast<AbstractProjectItem *>(parent.internalPointer());
        while (!parentItem->isFolder()) {
            parentItem = parentItem->parent();
        }
    }
    else {
        parentItem = m_rootFolder;
    }
    foreach(const QString &id, ids) {
        ProjectClip *currentItem = m_rootFolder->clip(id);
        AbstractProjectItem *currentParent = currentItem->parent();
        if (currentParent != parentItem) {
            // Item was dropped on a different folder
            currentParent->removeChild(currentItem);
            currentItem->setParent(parentItem);
        }
    }
}

void Bin::slotItemDropped(const QList<QUrl>&urls, const QModelIndex &parent)
{
    QStringList folderInfo;
    if (parent.isValid()) {
        // Check if drop occured on a folder
        AbstractProjectItem *parentItem = static_cast<AbstractProjectItem *>(parent.internalPointer());
        while (!parentItem->isFolder()) {
            parentItem = parentItem->parent();
        }
        if (parentItem != m_rootFolder) {
            folderInfo << parentItem->name();
            folderInfo << parentItem->clipId();
        }
    }
    //TODO: verify if urls exist, check for folders
    ClipCreationDialogDialog::createClipsCommand(pCore->projectManager()->current(), urls, folderInfo, this);
}

